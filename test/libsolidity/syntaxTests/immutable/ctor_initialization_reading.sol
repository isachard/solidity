contract C {
    uint immutable x;
    constructor() public {
        x = 3 + x;
    }
}
// ----
// TypeError: (78-79): Immutable variables cannot be read in the constructor or any function or modifier called by it.
