contract C {
    uint immutable x;
    constructor() initX public {
    }

    modifier initX() {
        _; x = 23;
    }
}
// ----
// TypeError: (109-110): Immutable variables can only be initialized directly in the constructor.
