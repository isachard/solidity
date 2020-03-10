contract C {
    uint immutable x = f();

    function f() public pure returns (uint) { return 3 + x; }
}
// ----
// TypeError: (99-100): Immutable variables cannot be read in the constructor or any function or modifier called by it.
