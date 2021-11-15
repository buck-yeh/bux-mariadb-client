#pragma once

/*! \file
    \brief MySQL/MariaDB Connector/C API Throw-On-Error C++ Wrappers
*/

#include <mysql/mysql.h>    // MYSQL, MYSQL_RES, MYSQL_STMT, MYSQL_BIND
#include <concepts>         // std::integral<>, std::convertible_to<>, std::invocable<>
#include <functional>       // std::function<>
#include <limits>           // std::numeric_limits<>
#include <map>              // std::map<>
#include <memory>           // std::unique_ptr<>
#include <optional>         // std::optional<>
#include <string>           // std::string

namespace bux {

//
//      Types
//
enum E_MySqlResultKind
/// \brief Retrieval methods after calling query() based using MYSQL_RES API
{
    MYSQL_USE_RESULT,   ///< mysql_use_result()
    MYSQL_STORE_RESULT  ///< mysql_store_result()
};

class [[nodiscard]]C_MySqlResult
/*! \brief Owner class of <a href="https://dev.mysql.com/doc/refman/5.7/en/mysql-use-result.html">MYSQL_RES *</a>
    which is intended to be directly passed to original MySQL API, e.g. <tt>mysql_fetch_row()</tt>
*/
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
/// \brief Owner class of <a href="https://dev.mysql.com/doc/refman/5.7/en/c-api-prepared-statement-functions.html">MYSQL_STMT *</a> with recurring methods
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
    void exec() const;
    MYSQL_BIND *execBindResults(const std::function<void(MYSQL_BIND *barr)> &binder);
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
    std::string             m_host, m_user, m_password, m_db, m_charset = "utf8mb4";
    std::optional<unsigned> m_port;
};

class C_MySQL
/*! \brief <tt>MYSQL*</tt> wrapper class, which is thread-aware but not thread-safe.
            Use mutex to guard the use of C_MySQL instance or there will be trouble.
*/
{
public:

    // Nonvirtuals
    C_MySQL(std::invocable<> auto getConnArg) requires requires { {getConnArg()}->std::convertible_to<C_MyConnectArg>; }:
        m_getConnArg(getConnArg) {}
    C_MySQL(std::convertible_to<C_MyConnectArg> auto connArg):
        m_getConnArg([connArg]{ return connArg; }) {}
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
    std::function<C_MyConnectArg()> const m_getConnArg;
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
template<std::integral T>
void bindInt(MYSQL_BIND &dst, T &value)
{
#if 1
    volatile auto t = &dst.is_null_value;
    dst.is_null = t; // The trick is due to the straightforward form as in else part is somehow optimized away.
#else
    dst.is_null = &dst.is_null_value;
#endif
    dst.buffer_type = typeOfIntSize(sizeof value);
    dst.is_unsigned = !(std::numeric_limits<T>::is_signed);
    dst.buffer = &value;
    dst.buffer_length = sizeof value;
}

} // namespace bux
