#include <mariadb/mysql.h>
#include <string>

static const std::string sql_all_unaccounted = "SELECT cluster from cluster WHERE allocated = 0 order by cluster asc";
static const std::string sql_next_empty = "SELECT cluster FROM cluster WHERE allocated = 1 order by cluster asc limit 1";
int main() {
    MYSQL_RES *res = nullptr;
    MYSQL *mysql = mysql_init(nullptr), *mysql2 = mysql_init(nullptr);
    //mysql_optionsv(mysql, MYSQL_READ_DEFAULT_FILE, nullptr);
    mysql_real_connect(mysql, nullptr, "root", "root", "resurrex", 0, "/run/mysqld/mysqld.sock", 0);
    mysql_real_query(mysql, sql_all_unaccounted.c_str(), sql_all_unaccounted.length());
    for (;;) {
        if (mysql_next_result(mysql)) {
            break;
        }
        res = mysql_use_result(mysql);
        res->fields->
    }

    while (!mysql_next_result_start(int *ret, MYSQL *mysql))
}
