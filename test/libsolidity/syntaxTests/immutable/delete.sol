contract C {
    uint immutable x = 3;
    constructor() public {
        delete x;
    }
}
// ----
// TypeError: (81-82): Immutable variables cannot be read in the constructor or any function or modifier called by it.
