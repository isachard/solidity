contract C {
    uint immutable x;
    constructor() public {
        if (false)
            return;

        x = 1;
    }
}
// ----
// TypeError: (93-100): Construction controlflow ends without initializing all immutable state variables.
