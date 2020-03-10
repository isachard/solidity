contract C {
    uint immutable x = 3;
    constructor() public {
        x++;
    }
}
// ----
// TypeError: (74-75): Immutable variables cannot be read in the constructor or any function or modifier called by it.
