#include <string>
#include <iostream>

#include "Connection.h"
#include "Public.h"


Connection::Connection() {
  conn_ = mysql_init(nullptr);
}

Connection::~Connection() {
  if (conn_ != nullptr)
    mysql_close(conn_);
}

bool Connection::connect(string ip, unsigned short port, string user,
                         string password, string dbname) {
  MYSQL *p = mysql_real_connect(conn_, ip.c_str(), user.c_str(), password.c_str(),
                                dbname.c_str(), port, nullptr, 0);
  return p != nullptr;
}

bool Connection::update(string sql) {
  if (mysql_query(conn_, sql.c_str())) {
    LOG("更新失败:" + sql);
    return false;
  }
  return true;
}

MYSQL_RES *Connection::query(string sql) {
  if (mysql_query(conn_, sql.c_str())) {
    LOG("查询失败:" + sql);
    return nullptr;
  }
  return mysql_use_result(conn_);
}

void Connection::refreshAliveTime() {
  aliveTime_ = clock();
}

clock_t Connection::getAliveTime() const {
  return clock() - aliveTime_;
}