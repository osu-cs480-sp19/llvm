#include <iostream>

extern "C" {
 // int addRecursive(int a, int b);
 float foo();
}

int main(int argc, char const *argv[]) {
  // int c = addRecursive(8, 16);
  float c = foo();
  std::cout << "c = " << c << std::endl;
  return 0;
}
