int os_is_linux() {
#ifdef __linux__
  return 1;
#else
  return 0;
#endif
}
int os_is_apple() {
#ifdef __APPLE__
  return 1;
#else
  return 0;
#endif
}
