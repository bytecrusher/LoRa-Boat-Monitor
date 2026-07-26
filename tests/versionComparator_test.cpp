#include "versionComparator.h"

#include <assert.h>

int main() {
  assert(compareFirmwareVersionStrings("V1.14z", "V1.14aa") < 0);
  assert(compareFirmwareVersionStrings("V1.14aa", "V1.14ab") < 0);
  assert(compareFirmwareVersionStrings("V1.14q", "V1.14q") == 0);
  assert(compareFirmwareVersionStrings("v1.14Q", "V1.14q") == 0);
  assert(compareFirmwareVersionStrings("V1.14", "V1.14a") < 0);
  assert(compareFirmwareVersionStrings("V1.15a", "V1.14zz") > 0);
  assert(compareFirmwareVersionStrings("V2.1a", "V1.99zz") > 0);
  assert(compareFirmwareVersionStrings("invalid-b", "invalid-a") > 0);
  return 0;
}
