#include <bux/oo_mariadb.h>
#include <bux/XException.h> // LOGIC_ERROR(), RUNTIME_ERROR()
#include <cstring>          // memset()
#include <vector>           // std::vector<>
#include <algorithm>        // std::min()
#ifdef CLT_DEBUG_
#include <bux/Logger.h>     // LOG(), FUNLOGX()
#endif

/* NOTE:
 * TIMESTAMP(fsp) to double: unix_timestamp(?)
 * double to TIMESTAMP(fsp): from_unixtime(?)
 * DATE to int: (unix_timestamp(?)+TIMEDIFF(NOW(), UTC_TIMESTAMP)/10000*3600)/86400
 * int to DATE: from_unixtime(?*86400-TIMEDIFF(NOW(), UTC_TIMESTAMP)/10000*3600)
 */
namespace {

//
//      In-Module Functions
//
void flushResults(MYSQL *mysql) noexcept
{
    while (!mysql_next_result(mysql))
        mysql_free_result(mysql_use_result(mysql));
}

} // namespace

namespace bux {

//
//      Functions
//
std::string errorSuffix(MYSQL *mysql)
{
    std::string ret;
    if (auto err = mysql_errno(mysql))
    {
        ret = std::format(" with mysql error({})[{}]", err, mysql_sqlstate(mysql));
        auto msg = mysql_error(mysql);
        if (*msg)
            ret.append(" \"").append(msg) += '\"';
    }
    return ret;
}

std::string errorSuffix(MYSQL_STMT *stmt)
{
    std::string ret;
    if (auto err = mysql_stmt_errno(stmt))
        ret = std::format(" with mysql stmt error({}): {}", err, mysql_stmt_error(stmt));

    mysql_stmt_free_result(stmt);
    return ret;
}

void query(MYSQL *mysql, const std::string &sql)
{
    flushResults(mysql);
Retry:
    if (mysql_query(mysql, sql.c_str()))
        switch (mysql_errno(mysql))
        {
        case 1205: // From MySQL: "Lock wait timeout exceeded; try restarting transaction"
        case 1213: // From MySQL: "Deadlock found when trying to get lock; try restarting transaction"
            goto Retry;
        default:
            RUNTIME_ERROR("Query \"{}\"{}", sql, errorSuffix(mysql));
        }
}

void affect(MYSQL *mysql, const std::string &sql)
{
    query(mysql, sql);
    switch (mysql_affected_rows(mysql))
    {
    case MYSQL_COUNT_ERROR:
        RUNTIME_ERROR("Affected \"{}\"{}", sql, errorSuffix(mysql));
    case 0:
        RUNTIME_ERROR("Zero affected row by \"{}\"", sql);
    default:;
    }
}

C_MySqlResult query(MYSQL *mysql, const std::string &sql, E_MySqlResultKind kind)
{
    query(mysql, sql);
    MYSQL_RES *res;
    switch (kind)
    {
    case MYSQL_USE_RESULT:
        res = mysql_use_result(mysql);
        break;
    case MYSQL_STORE_RESULT:
        res = mysql_store_result(mysql);
        break;
    default:
        RUNTIME_ERROR("Unknown result kind");
    }
    if (!res)
    {
        std::string err;
        if (mysql_errno(mysql))
            err = "Fail to store result"+errorSuffix(mysql);
        else
            err = "No result of '"+sql+'\'';

        RUNTIME_ERROR("{}", err);
    }
    return res;
}

void queryColumn(MYSQL *mysql, const std::string &sql, std::function<bool(const char*)> nextRow, int colInd)
{
    const auto res = query(mysql, sql, MYSQL_USE_RESULT);
    while (auto row = mysql_fetch_row(res))
        if (!nextRow(row[colInd]))
            break;
}

std::string queryString(MYSQL *mysql, const std::string &sql, int colInd)
{
    std::string ret;
    queryColumn(mysql, sql, [&ret](const char *s) {
            if (s)
            {
                ret = s;
                return false;
            }
            return true;
        }, colInd);
    return ret;
}

unsigned long queryULong(MYSQL *mysql, const std::string &sql, int colInd)
{
    unsigned long ret = 0;
    queryColumn(mysql, sql, [&ret](const char *s) {
        if (s)
        {
            char *end;
            ret = strtoul(s, &end, 0);
            if (*end)
                RUNTIME_ERROR("Not unsigned integer");

            return false;
        }
        return true;
    }, colInd);
    return ret;
}

std::string getTableSchema(MYSQL *mysql, const std::string &db_name, const std::string &table_name)
{
    const std::string db_prefix = '`' + db_name + "`.";
    auto ret = queryString(mysql, "show create table "+db_prefix+table_name, 1);
    for (size_t pos; pos = ret.find(db_prefix), pos != std::string::npos;)
         ret.erase(pos, db_prefix.size());
         // When parent table(s)/view(s) doesn't exist, db-qualifiers will always show
         // even if it is the current database.

    return ret;
}

bool isCaseSensitive(MYSQL *mysql)
{
    switch (auto type = queryULong(mysql, "show variables like 'lower\\_case\\_table\\_names'", 1))
    {
    case 0: // Unix-like: case-sensitive
        return true;
    case 1: // Windows: case-insensitive
    case 2: // Mac OS X: Convert to lower cases and then compare
        return false;
    default:
        RUNTIME_ERROR("Unexpected lower_case_table_names value {}", type);
    }
}

void getDatabaseCollation(MYSQL *mysql, const std::string &bof_db,
    std::function<void(const char *charset, const char *collate)> apply)
{
    const auto res = query(mysql,
        "select DEFAULT_CHARACTER_SET_NAME,DEFAULT_COLLATION_NAME from INFORMATION_SCHEMA.SCHEMATA where SCHEMA_NAME='"+bof_db+'\'',
        MYSQL_USE_RESULT);
    if (auto row = mysql_fetch_row(res))
        apply(row[0], row[1]);
}

std::string getCloneDatabaseOptions(MYSQL *mysql, const std::string &bof_db)
{
    std::string ret;
    getDatabaseCollation(mysql, bof_db,
        [&](const char *charset, const char *collate) {
            ret.assign(" character set '").append(charset).append("' collate '").append(collate) += '\'';
        });
    return ret;
}

void resetDatabase(C_MySQL &mysql, const std::string &db_name, const std::string &bof_db)
{
    std::string extra;
    if (!bof_db.empty())
        extra = getCloneDatabaseOptions(mysql, bof_db);

    query(mysql, "drop database if exists "+db_name);
    affect(mysql, "create database "+db_name+extra);
}

void useDatabase(MYSQL *mysql, const std::string &db_name)
{
#ifdef CLT_DEBUG_
    FUNLOGX(db_name);
#endif
    if (mysql_select_db(mysql, db_name.c_str()))
        RUNTIME_ERROR("Use database {}{}", db_name, errorSuffix(mysql));
}

void bindLongBlob(MYSQL_BIND &dst)
{
    dst.is_null = &dst.is_null_value;
    dst.length = &dst.length_value;
    dst.buffer_type = MYSQL_TYPE_LONG_BLOB;
}

void bindStrBuffer(MYSQL_BIND &dst, char *str, size_t bytes)
{
    dst.is_null = &dst.is_null_value;
    dst.length = &dst.length_value;
    dst.buffer_type = MYSQL_TYPE_STRING;
    dst.buffer = str;
    dst.buffer_length = static_cast<unsigned long>(bytes);
}

void bindStrParam(MYSQL_BIND &dst, const char *str, size_t bytes)
{
    dst.length_value =
    dst.buffer_length = static_cast<unsigned long>(bytes);
    dst.buffer_type = MYSQL_TYPE_STRING;
    dst.buffer = const_cast<char*>(str);
}

void bindStrParam(MYSQL_BIND &dst, const std::string &str)
{
    bindStrParam(dst, str.data(), str.size());
}

const char *endStr(MYSQL_BIND &dst)
{
    char *const ret = static_cast<char*>(dst.buffer);
    ret[dst.is_null_value? 0: dst.length_value] = 0;
    return ret;
}

enum_field_types typeOfIntSize(size_t n)
{
    enum_field_types ret;
    switch (n)
    {
    case 1:
        ret = MYSQL_TYPE_TINY;
        break;
    case 2:
        ret = MYSQL_TYPE_SHORT;
        break;
    case 4:
        ret = MYSQL_TYPE_LONG;
        break;
    case 8:
        ret = MYSQL_TYPE_LONGLONG;
        break;
    default:
        LOGIC_ERROR("Integer of {} bytes", n);
    }
    return ret;
}

//
//      Implemen Classes
//
C_MySQL::~C_MySQL()
{
    disconnect();
}

MYSQL *C_MySQL::mysql()
{
    if (m_pstmt)
        m_pstmt->clear();

    if (m_mysql &&
        (flushResults(m_mysql), !mysql_ping(m_mysql)))
    {
        const auto cur_id = mysql_thread_id(m_mysql);
        if (cur_id != m_threadID)
        {
#ifdef CLT_DEBUG_
            LOG(LL_INFO, "mysql_ping() obtained new thread id {} obsoleting the old {}", cur_id, m_threadID);
#endif
            m_threadID = cur_id;
            m_pstmt.reset();
        }
    }
    else
        connect_();

    return m_mysql;
}

void C_MySQL::connect_()
{
#ifdef CLT_DEBUG_
    if (!m_mysql)
        LOG(LL_VERBOSE, "About to connect to MySQL");
    else
        LOG(LL_ERROR, "mysql_ping() failed{}", errorSuffix(m_mysql));
#endif
    disconnect();

    MYSQL *const mysql = mysql_init(nullptr);
    if (!mysql)
        LOGIC_ERROR("mysql_init() failed");

    const auto arg = m_getConnArg();
    const char *whatPrefix;
    if (mysql_options(mysql, MYSQL_SET_CHARSET_NAME, arg.m_charset.c_str()))
        whatPrefix = "Fail to set charset";
    else if (mysql_options(mysql, MYSQL_OPT_RECONNECT, "1"))
        whatPrefix = "Fail to enable auto-reconnect";
    else
    {
        const char *password{};
        if (!arg.m_password.empty())
            password = arg.m_password.c_str();

        if (mysql_real_connect(mysql, arg.m_host.c_str(), arg.m_user.c_str(), password,
            arg.m_db.empty()? nullptr: arg.m_db.c_str(),
            arg.m_port? *arg.m_port: 0,
            nullptr, CLIENT_MULTI_STATEMENTS|CLIENT_COMPRESS))
            // Connected successfully
        {
            m_mysql = mysql;
            query(mysql, "SET sql_mode = 'STRICT_ALL_TABLES'");
            m_threadID = mysql_thread_id(mysql);
#ifdef CLT_DEBUG_
            LOG(LL_INFO, "Connected to MySQL on {} as user '{}' and thread id {}", arg.m_host, arg.m_user, m_threadID);
#endif
            return;
        }
        whatPrefix = "Fail to connect";
    }

    // Something went wrong
    const std::string err = errorSuffix(mysql);
    mysql_close(mysql);
    RUNTIME_ERROR("{}{}", whatPrefix, err);
}

void C_MySQL::disconnect()
{
    m_pstmt.reset();
    if (m_mysql)
    {
        mysql_close(m_mysql);
        m_mysql = nullptr;
    }
}

C_MySqlStmt &C_MySQL::stmt()
{
    MYSQL *const my = mysql();  // trigger mysql_ping()
    if (!m_pstmt)
        m_pstmt.reset(new C_MySqlStmt(my));

    return *m_pstmt;
}

unsigned long C_MySQL::threadId()
{
    if (!m_mysql)
        connect_();

    return m_threadID;
}

void C_MySqlResult::operator=(C_MySqlResult &&t) noexcept
{
    destroy();
    m_res = t.m_res;
    t.m_res = {};
}

void C_MySqlResult::destroy()
{
    if (m_res)
        mysql_free_result(m_res);
}

C_MySqlStmt::C_MySqlStmt(MYSQL *mysql): m_stmt(mysql_stmt_init(mysql))
{
    if (!m_stmt)
        RUNTIME_ERROR("Fail to init stmt{}", errorSuffix(mysql));
}

C_MySqlStmt::~C_MySqlStmt()
{
    if (m_stmt)
        mysql_stmt_close(m_stmt);
}

bool C_MySqlStmt::affected() const
{
    return static_cast<int>(mysql_stmt_affected_rows(m_stmt)) > 0;
}

void C_MySqlStmt::allocBind(size_t count)
{
    if (m_bindSizeLimit < count)
    {
        m_bindSizeLimit = count;
        m_bindArr = std::make_unique<MYSQL_BIND[]>(count);
    }

    m_bindSize = count;
    if (count)
        memset(m_bindArr.get(), 0, sizeof(MYSQL_BIND)*count);
    else
    {
        m_bindArr.reset();
        m_bindSizeLimit = 0;
    }
}

void C_MySqlStmt::bindParams(const std::function<void(MYSQL_BIND *barr)> &binder)
{
    allocBind(mysql_stmt_param_count(m_stmt));
    const auto barr = bindArray();
    binder(barr);
    std::vector<size_t> longParams;
    for (size_t i = 0; i < m_bindSize; ++i)
        if (barr[i].buffer_length > maxAllowedPacket())
            longParams.emplace_back(i);

    if (mysql_stmt_bind_param(m_stmt, barr))
        RUNTIME_ERROR("Fail to bind params{}", errorSuffix(m_stmt));

    for (auto i: longParams)
    {
        const MYSQL_BIND &src = barr[i];
        const auto step = maxAllowedPacket();
        for (unsigned off = 0; off < src.buffer_length; off += step)
        {
            const auto bytes = std::min(step, unsigned(src.buffer_length-off));
            if (mysql_stmt_send_long_data(m_stmt, unsigned(i), static_cast<const char*>(src.buffer)+off, bytes))
                RUNTIME_ERROR("Fail to send long data part of {} bytes", bytes);
        }
    }
}

void C_MySqlStmt::clear() const
{
    mysql_stmt_free_result(m_stmt);
}

void C_MySqlStmt::exec() const
{
    if (execNoThrow())
        RUNTIME_ERROR("Fail to execute{}",errorSuffix(m_stmt));
}

unsigned C_MySqlStmt::execNoThrow() const
{
Retry:
    if (mysql_stmt_execute(m_stmt))
    {
        switch (auto ret = mysql_stmt_errno(m_stmt))
        {
        case 1213:
            // From MySQL: "Deadlock found when trying to get lock; try restarting transaction"
            goto Retry;
        default:
            return ret;
        }
    }
    return 0;
}

MYSQL_BIND *C_MySqlStmt::execBindResults(const std::function<void(MYSQL_BIND *barr)> &binder)
{
    exec();
    allocBind(mysql_stmt_field_count(m_stmt));
    const auto barr = bindArray();
    binder(barr);
    if (mysql_stmt_bind_result(m_stmt, barr))
        RUNTIME_ERROR("Fail to bind result{}", errorSuffix(m_stmt));

    return barr;
}

std::pair<const void*,size_t> C_MySqlStmt::getLongBlob(size_t i, std::function<void*(size_t bytes)> alloc) const
{
    MYSQL_BIND bindBlob;
    memset(&bindBlob, 0, sizeof bindBlob);
    auto &bind = bindArray()[i];
    if (bind.is_null_value)
        return {{}, 0};

    bindBlob.buffer = alloc(bind.length_value);
    m_stmt->bind[i].length_value =
    bindBlob.buffer_length = bind.length_value;
    bindBlob.length = &bindBlob.length_value;
    bindBlob.buffer_type = bind.buffer_type;
    if (mysql_stmt_fetch_column(m_stmt, &bindBlob, static_cast<unsigned>(i), 0))
        RUNTIME_ERROR("Fail to fetch blob data{}", errorSuffix(m_stmt));

    return {bindBlob.buffer, bind.length_value};
}

std::string C_MySqlStmt::getLongBlob(size_t i) const
{
    std::unique_ptr<char[]> buf;
    const size_t n = getLongBlob(i,
        [&buf](size_t bytes) {
            buf = std::make_unique<char[]>(bytes);
            return buf.get();
        }
    ).second;
    return {buf.get(), n};
}

unsigned C_MySqlStmt::maxAllowedPacket() const
{
    if (!m_maxPacketBytes)
    {
        C_MySqlStmt stmt(m_stmt->mysql);    // Access of m_stmt->mysql is undocumented
        stmt.prepare("select @@max_allowed_packet");
        stmt.execBindResults([this](auto barr){
            bindInt(*barr, m_maxPacketBytes);
        });
        if (!stmt.nextRow() || m_maxPacketBytes % 1024)
            // Null or odd answer
            m_maxPacketBytes = 65536;

        m_maxPacketBytes /= 2;
    }
    return m_maxPacketBytes;
}

bool C_MySqlStmt::nextRow() const
{
    const int err = mysql_stmt_fetch(m_stmt);
    if (err == 1)
        RUNTIME_ERROR("Fail to fetch row{}", errorSuffix(m_stmt));

    return err != MYSQL_NO_DATA;
}

void C_MySqlStmt::prepare(const std::string &sql) const
{
    if (mysql_stmt_prepare(m_stmt, sql.c_str(), static_cast<unsigned long>(sql.size())))
        RUNTIME_ERROR("Prepare \"{}\"{}", sql, errorSuffix(m_stmt));
}

bool C_MySqlStmt::queryUint(unsigned &dst)
{
    execBindResults([&](auto barr){
        bindInt(*barr, dst);
    });
    if (!nextRow())
        return false;

    return !bindArray()->is_null_value;
}

C_LockTablesTillEnd::C_LockTablesTillEnd(C_MySQL &mysql):
    m_mysql(mysql),
    m_state(LS_NONE)
{
}

void C_LockTablesTillEnd::addRead(const std::string &table)
{
    m_spec[table] = "read";
}

void C_LockTablesTillEnd::addWrite(const std::string &table)
{
    m_spec[table] = "write";
}

void C_LockTablesTillEnd::lock()
{
    if (m_spec.empty())
        return unlock();

    std::string sql;
    for (auto &i: m_spec)
    {
        if (sql.empty())
            sql = "lock tables ";
        else
            sql += ", ";

        sql.append(i.first).append(1, ' ').append(i.second);
    }
    query(m_mysql, sql);
    m_state = LS_BY_SPEC;
}

void C_LockTablesTillEnd::lockAllRead()
{
    if (m_state != LS_ALL_READ)
    {
        query(m_mysql, "FLUSH TABLES WITH READ LOCK");
        m_state = LS_ALL_READ;
    }
}

void C_LockTablesTillEnd::remove(const std::string &table)
{
    m_spec.erase(table);
}

void C_LockTablesTillEnd::removeAll()
{
    m_spec.clear();
}

void C_LockTablesTillEnd::unlock()
{
    if (m_state != LS_NONE)
    {
        query(m_mysql, "unlock tables");
        m_state = LS_NONE;
    }
}

} // namespace bux
