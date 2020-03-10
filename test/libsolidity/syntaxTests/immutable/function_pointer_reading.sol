contract B {
    uint immutable x;

    constructor(function() internal returns(uint) fp) internal {
        x = fp();
    }
}

contract C is B(C.f) {
    function f() internal returns(uint) { return x + 2; }
}
// ----
// TypeError: (200-201): Immutable variables cannot be read in the constructor or any function or modifier called by it.
