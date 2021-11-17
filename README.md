# Rationale
The goal is to minimize boilerplate code when doing the same things using Connector/C. Only recurring usages are wrapped into helper classes C++ functions. Mixed uses of this module and Connector/C API are expected. Remember to always use `bux::C_MySQL`, `bux::C_MySqlStmt`, `bux::C_MySqlResult` over `MYSQL*`, `MYSQL_STMT*`, `MYSQL_RES*` and you will be fine.  

# API Summary
## Recurring Types and Their Safe Counterparts
| Original Type | Wrapper Class | 
|:-------------:|:--------------|
| `MYSQL*`      |  `bux::C_MySQL` |
| `MYSQL_STMT*` |  `bux::C_MySqlStmt` |
| `MYSQL_RES*`  |  `bux::C_MySqlResult` |

### Benefits
1. The right colum of each row above can be implicitly cast to the left column, safely. You are welcome!
2. `bux::C_MySQL` can only be constructed by a connction parameter generator funtion, hence it will automatically connect when being _implicitly_ or _explicitly_ cast to `MYSQL*`
3. `bux::C_MySQL` can also be implicitly cast to `MYSQL_STMT*`, on behalf of its underlying single `bux::C_MySqlStmt` instance.

## MYSQL_STMT 
`MYSQL_STMT*` releated function are highly wrapped into methods of `bux::C_MySqlStmt`. Usages against alternatives are recommended to prevent _SQL injection_.

## Funtions
- If your just need to get an single value from SQL, like:
  ~~~SQL
  select concat(substr(18999000000, 1, 11-length(100)), 100);
  ~~~
  Just call `queryString()` to get `std::string` value or call `queryULong()` to get `unsigned long` value.
- If only one column of mutiple-row result are concerned, call `queryColumn()`
- In general form, call 
  ~~~C++
  C_MySqlResult query(MYSQL *mysql, const std::string &sql, E_MySqlResultKind kind);
  ~~~ 
  to get string values of multiple columns of multiple rows. Though `bux::C_MySqlStmt` methods are usually the better alternatives to be considered.
- If you want to change something, call the casual form
  ~~~SQL
  void query(MYSQL *mysql, const std::string &sql);
  ~~~
  which doesn't care if the change eventually takes effect. Or call 
  ~~~SQL
  void affect(MYSQL *mysql, const std::string &sql);
  ~~~
  which throws `std::runtime_error` if the change doesn't happen.
- The `bind\w+(MYSQL_BIND &dst, ...)` functions are expected to be called within callback functions provided as paramter of either `bux::C_MySqlStmt::bindParams()` or `bux::C_MySqlStmt::execBindResults()`
  ~~~C++
  template<std::integral T>
  void bindInt(MYSQL_BIND &dst, T &value);
  void bindLongBlob(MYSQL_BIND &dst);

  // For C_MySqlStmt::execBindResults() only
  void bindStrBuffer(MYSQL_BIND &dst, char *str, size_t bytes);

  // For C_MySqlStmt::bindParams() only  
  void bindStrParam(MYSQL_BIND &dst, const char *str, size_t bytes);
  void bindStrParam(MYSQL_BIND &dst, std::string &&str) = delete; // ban temporary string by link error
  void bindStrParam(MYSQL_BIND &dst, const std::string &str);
  ~~~
- Call `useDatabase()` to change the _current_ database.
- Call `getTableSchema()` to get table schema in form of `CREATE TABLE ...` SQL command.
- Call `resetDatabase()` to clear the whole database, into `empty`, _with extreme care_.