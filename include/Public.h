#ifndef PUBLIC_H
#define PUBLIC_H
#define LOG(str) \
  cout << __FILE__ << ": " << __LINE__ << " " << \
  __TIMESTAMP__ << " : " << str << '\n';

#endif