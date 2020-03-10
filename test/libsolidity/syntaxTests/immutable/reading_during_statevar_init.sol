contract C {
    uint immutable x = 0;
    uint y = x;
}
// ----
// TypeError: (52-53): Immutable variables cannot be read in the constructor or any function or modifier called by it.
