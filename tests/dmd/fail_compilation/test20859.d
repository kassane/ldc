/** DISABLED: LDC // succeeds, as LDC doesn't add a (undocumented) magic __vtbl member variable (unused `ClassDeclaration.vtblSymbol()`)
TEST_OUTPUT:
---
fail_compilation/test20859.d(8): Error: variable `test20859.ICE.__vtbl` conflicts with variable `test20859.ICE.__vtbl` at fail_compilation/test20859.d(10)
---
*/

class ICE
{
    void **__vtbl;
}
