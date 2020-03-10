contract C {
    uint immutable x;
}
// ----
// TypeError: (0-36): Construction controlflow ends without initializing all immutable state variables.
