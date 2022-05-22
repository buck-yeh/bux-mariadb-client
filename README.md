
## Table of Contents

   * [Rationale](#rationale)
   * [API Summary](#api-summary)
      * [Recurring Types and Their Safe Counterparts](#recurring-types-and-their-safe-counterparts)
         * [Benefits](#benefits)
      * [MYSQL_STMT](#mysql_stmt)
      * [Funtions](#funtions)
   * [Installation](#installation)
      * [in <a href="https://archlinux.org/" rel="nofollow">ArchLinux</a>](#in-archlinux)
      * [from github in any of <a href="https://distrowatch.com/" rel="nofollow">Linux distros</a>](#from-github-in-any-of-linux-distros)
      * [from vcpkg in Windows](#from-vcpkg-in-windows)

*(Created by [gh-md-toc](https://github.com/ekalinin/github-markdown-toc))*

## Rationale

The idea is to minimize boilerplate code when doing the same things using Connector/C. Only recurring usages are wrapped into helper classes or plain functions. Mixed uses of this module and Connector/C API are expected. Always prefer `bux::C_MySQL`, `bux::C_MySqlStmt`, `bux::C_MySqlResult` over `MYSQL*`, `MYSQL_STMT*`, `MYSQL_RES*` and you will be fine.  

## API Summary

### Recurring Types and Their Safe Counterparts

| Original Type | Wrapper Class | 
|:-------------:|:--------------|
| `MYSQL*`      |  `bux::C_MySQL` |
| `MYSQL_STMT*` |  `bux::C_MySqlStmt` |
| `MYSQL_RES*`  |  `bux::C_MySqlResult` |

#### Benefits

1. The right colum (_class type_) of each row above can be cast to the left column (_native MySQL pointer type_) implicitly & _safely_.
2. `bux::C_MySQL` can only be constructed by a connction parameter generator funtion, hence it will automatically connect when being _implicitly_ or _explicitly_ cast to `MYSQL*`
3. `bux::C_MySQL` can also be implicitly cast to `MYSQL_STMT*`, on behalf of its underlying single `bux::C_MySqlStmt` instance.

### MYSQL_STMT

`MYSQL_STMT*` releated function are highly wrapped into methods of `bux::C_MySqlStmt`, preferred over `bux::C_MySqlResult` to prevent _SQL injection_.

~~~C++
class C_MySqlStmt
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
    //...
};
~~~

### Funtions

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
- Call `resetDatabase()` to clear the whole database, into emptiness, _with extreme care_.

## Installation

### in [ArchLinux](https://archlinux.org/)

1. Make sure you have installed [`yay`](https://aur.archlinux.org/packages/yay/) or any other [pacman wrapper](https://wiki.archlinux.org/index.php/AUR_helpers).
2. `yay -S bux-mariadb-client` to install. `bux` is also installed with it.
3. `yay -Ql bux-mariadb-client` to see the installed files:

   ~~~bash
   bux-mariadb-client /usr/
   bux-mariadb-client /usr/include/
   bux-mariadb-client /usr/include/bux/
   bux-mariadb-client /usr/include/bux/oo_mariadb.h
   bux-mariadb-client /usr/lib/
   bux-mariadb-client /usr/lib/libbux-mariadb-client.a
   bux-mariadb-client /usr/share/
   bux-mariadb-client /usr/share/licenses/
   bux-mariadb-client /usr/share/licenses/bux-mariadb-client/
   bux-mariadb-client /usr/share/licenses/bux-mariadb-client/LICENSE
   ~~~

4. Include the sole header file by prefixing the header name with `bux/`:

   ~~~c++
   #include <bux/oo_mariadb.h>
   ~~~

   *p.s.* Compiler is expected to search `/usr/include` by default.
5. If directly using `gcc` or `clang` is intended, the required compiler flags are `-std=c++2a -lbux-mariadb-client -lbux`

### from github in any of [Linux distros](https://distrowatch.com/)

1. Make sure you have installed `cmake` `make` `gcc` `git` `fmt` `mariadb-libs`, or the likes. Known package names in different distros/package-managers:
   | Distro/PkgMngr | Package Name |
   |:----------------:|:------------:|
   | ArchLinux/yay | `fmt`, `mariadb-libs` |
   | Fedora/dnf | `fmt-devel`, `mariadb-connector-c-devel` |

2. ~~~bash
   git clone https://github.com/buck-yeh/bux-mariadb-client.git
   cd bux-mariadb-client
   cmake -D FETCH_DEPENDEES=1 -D DEPENDEE_ROOT=_deps .
   make -j
   ~~~

3. Include `include/bux/oo_mariadb.h` and link with `src/libbux-mariadb-client.a`

### from vcpkg in Windows

1. ~~~PowerShell
   PS F:\vcpkg> .\vcpkg.exe search bux-mariadb-client
   buck-yeh-bux-mariadb-client 1.0.1#1       Loose-coupled throw-on-error C++20 wrapper classes and utilities over mysq...
   The result may be outdated. Run `git pull` to get the latest results.

   If your port is not listed, please open an issue at and/or consider making a pull request:
    https://github.com/Microsoft/vcpkg/issues
   PS F:\vcpkg>
   ~~~

2. Available triplets are:

   ~~~PowerShell
   buck-yeh-bux-mariadb-client:x64-windows
   buck-yeh-bux-mariadb-client:x64-windows-static
   buck-yeh-bux-mariadb-client:x64-windows-static-md
   buck-yeh-bux-mariadb-client:x86-windows
   buck-yeh-bux-mariadb-client:x86-windows-static
   ~~~

3. Include the sole header file by prefixing the header name with `bux/`:

   ~~~c++
   #include <bux/oo_mariadb.h>
   ~~~
