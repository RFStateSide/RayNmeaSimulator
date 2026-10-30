Public default layout for isolated regression and acceptance

defaultLayout.json is generated from the explicit 35-widget recipe in
tests/create_test_layout.cpp through MainController::addWidgetAsComboboxTextChanged.
It is not copied or derived from an owner's saved layout or registry settings.

Recipe: seven generators (two engines, two transmissions, movement, weather, time),
ten navigation instruments, seven propulsion instruments, three environment
instruments, four tanks, and four electrical instruments. Engine/transmission
instances are 0 and 1. Coordinates are synthetic zeroes. SID/time/local offset are
fixed and numeric strings canonicalized after serialization. The clock starts at
day 20000 since 1970; a running clock will subsequently follow its normal behavior.

Use only as disposable test data, with isolated settings and --no-nmea for the
standalone application. No marine transport handler is created by the generator.
Application tests use a closed localhost proxy for weather requests.

Regeneration instructions are in docs/DEPENDENCIES.txt. Retain the controller-based
recipe when changing this fixture so it can be regenerated without private files.
