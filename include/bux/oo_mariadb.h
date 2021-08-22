#ifndef oo_mariadb_H_
#define oo_mariadb_H_

/*! \file
    \brief MySQL/MariaDB Connector/C API Wrappers
*/

#include <mysql/mysql.h>    // MYSQL, MYSQL_RES, MYSQL_STMT, MYSQL_BIND
#include <functional>       // std::function<>
#include <memory>           // std::unique_ptr<>
#include <string>           // std::string
#include <limits>           // std::numeric_limits<>
#include <map>              // std::map<>

namespace bux {

//
//      Types
//
enum E_MySqlResultKind
/// \brief query 後取得 MYSQL_RES 物件指標的方式
{
    MYSQL_USE_RESULT,   ///< mysql_use_result()
    MYSQL_STORE_RESULT  ///< mysql_store_result()
};

class [[nodiscard]]C_MySqlResult
/// \brief 方便使用 <a href="https://dev.mysql.com/doc/refman/5.7/en/mysql-use-result.html">MYSQL_RES 相關API</a>的類別
{
public:

    // Nonvirtuals
    C_MySqlResult(MYSQL_RES *res): m_res(res) {}
    ~C_MySqlResult() { destroy(); }
    C_MySqlResult(C_MySqlResult &&t): m_res(t.m_res) { t.m_res = {}; }
    void operator=(C_MySqlResult &&t) noexcept;
    operator MYSQL_RES*() const { return m_res; }

private:

    MYSQL_RES   *m_res;

    // Nonvirtuals
    void destroy();
};

class C_MySqlStmt
/// \brief 方便使用 <a href="https://dev.mysql.com/doc/refman/5.7/en/c-api-prepared-statement-functions.html">MYSQL_STMT 相關API</a> 的類別
{
public:

    // Nonvirtuals
    C_MySqlStmt(MYSQL *mysql);
    ~C_MySqlStmt();
    C_MySqlStmt(const C_MySqlStmt &stmt) = delete;
    C_MySqlStmt &operator=(const C_MySqlStmt &) = delete;
    operator MYSQL_STMT*() const { return m_stmt; }
    bool affected() const;
    auto bindSize() const { return m_bindSize; }
    void bindParams(const std::function<void(MYSQL_BIND *barr)> &binder);
    void clear() const;
    MYSQL_BIND *execResults(const std::function<void(MYSQL_BIND *barr)> &binder);
    void exec() const;
    unsigned execNoThrow() const;
    std::pair<const void*,size_t> getLongBlob(size_t i, std::function<void*(size_t bytes)> alloc) const;
    std::string getLongBlob(size_t i) const;
    bool nextRow() const;
    void prepare(const std::string &sql) const;
    bool queryUint(unsigned &dst);

private:

    // Data
    MYSQL_STMT              *const m_stmt;
    size_t                  m_bindSize{0}, m_bindSizeLimit{0};
    std::unique_ptr<MYSQL_BIND[]> m_bindArr;
    mutable unsigned        m_maxPacketBytes{0};

    // Nonvirtuals
    void allocBind(size_t count);
    MYSQL_BIND *bindArray() const { return m_bindArr.get(); }
    unsigned maxAllowedPacket() const;
};

struct C_MyConnectArg
{
    std::string         m_host, m_user, m_password;
};
typedef std::function<C_MyConnectArg()> F_ConnectArg;

class C_MySQL
{
public:

    // Nonvirtuals
    template<class T>
    C_MySQL(T &&getConnArg): m_getConnArg(std::forward<T>(getConnArg)) {}
    ~C_MySQL();
    C_MySQL(const C_MySQL&) = delete;
    C_MySQL &operator=(const C_MySQL&) = delete;
    void disconnect();
    auto dup() const { return std::make_unique<C_MySQL>(m_getConnArg); }
    MYSQL *mysql();
    C_MySqlStmt &stmt();
    unsigned long threadId();

    // Type Erasors
    operator MYSQL*()           { return mysql(); }
    operator MYSQL_STMT*()      { return stmt(); }
    operator C_MySqlStmt&()     { return stmt(); }

protected:

    // Virtuals
    virtual void connect_();

private:

    // Data
    const F_ConnectArg              m_getConnArg;
    MYSQL                           *m_mysql{};
    unsigned long                   m_threadID{};
    std::unique_ptr<C_MySqlStmt>    m_pstmt;
};

class C_LockTablesTillEnd
{
public:

    // Nonvirtuals
    C_LockTablesTillEnd(C_MySQL &mysql);
    C_LockTablesTillEnd(const C_LockTablesTillEnd &) = delete;
    C_LockTablesTillEnd &operator=(const C_LockTablesTillEnd &) = delete;
    ~C_LockTablesTillEnd() { unlock(); }
    void addRead(const std::string &table);
    void addWrite(const std::string &table);
    void lock();
    void lockAllRead();
    void remove(const std::string &table);
    void removeAll();
    void unlock();
    auto &mysql() const { return m_mysql; }

private:

    // Types
    enum E_LockState
    {
        LS_NONE,
        LS_BY_SPEC,
        LS_ALL_READ
    };

    // Data
    C_MySQL                             &m_mysql;
    std::map<std::string,const char*>   m_spec;
    E_LockState                         m_state;
};

//
//      Externs
//
std::string errorSuffix(MYSQL *mysql);
std::string errorSuffix(MYSQL_STMT *stmt);

void query(MYSQL *mysql, const std::string &sql);
void affect(MYSQL *mysql, const std::string &sql);
void resetDatabase(C_MySQL &mysql, const std::string &db_name, const std::string &bof_db);
void useDatabase(MYSQL *mysql, const std::string &db_name);

bool isCaseSensitive(MYSQL *mysql);
std::string queryString(MYSQL *mysql, const std::string &sql, int colInd = 0);
unsigned long queryULong(MYSQL *mysql, const std::string &sql, int colInd = 0);
void queryColumn(MYSQL *mysql, const std::string &sql, std::function<bool(const char*)> nextRow, int colInd = 0);
C_MySqlResult query(MYSQL *mysql, const std::string &sql, E_MySqlResultKind kind);
std::string getTableSchema(MYSQL *mysql, const std::string &db_name, const std::string &table_name);

void bindLongBlob(MYSQL_BIND &dst);
void bindStrBuffer(MYSQL_BIND &dst, char *str, size_t bytes);
void bindStrParam(MYSQL_BIND &dst, const char *str, size_t bytes);
void bindStrParam(MYSQL_BIND &dst, std::string &&str) = delete; // ban temporary string by link error
void bindStrParam(MYSQL_BIND &dst, const std::string &str);
const char *endStr(MYSQL_BIND &dst);
enum_field_types typeOfIntSize(size_t n);

std::string getCloneDatabaseOptions(MYSQL *mysql, const std::string &bof_db);
void getDatabaseCollation(MYSQL *mysql, const std::string &bof_db,
    std::function<void(const char *charset, const char*collate)> apply);

//
//      Templates
//
template<class T>
void bindInt(MYSQL_BIND &dst, T &value)
{
#if 1
    volatile auto t = &dst.is_null_value;
    dst.is_null = t; // The trick is due to straightforward form is somehow optimized away.
#else
    dst.is_null = &dst.is_null_value;
#endif
    dst.buffer_type = typeOfIntSize(sizeof value);
    dst.is_unsigned = !(std::numeric_limits<T>::is_signed);
    dst.buffer = &value;
    dst.buffer_length = sizeof value;
}

} // namespace bux

#endif // oo_mariadb_H_
