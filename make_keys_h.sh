#!/bin/sh

cat <<'EOF'
// Axis codes from linux/input.h
static const code_map_t axis_codes[] = {
EOF

awk '
/ABS_(CNT|MAX|RESERVED)/ {next}
/#define ABS/ {printf "{\"%s\", %s},\n", $2, $2}
' /usr/include/linux/input-event-codes.h

cat <<'EOF'
  {NULL, 0},
};
EOF

cat <<'EOF'
// Button and key codes from linux/input.h
static const code_map_t button_codes[] = {
EOF

awk '
/KEY_(CNT|MAX|RESERVED|MIN_INTERESTING)/ {next}
/#define (BTN|KEY)/ {printf "{\"%s\", %s},\n", $2, $2}
' /usr/include/linux/input-event-codes.h

cat <<'EOF'
  {NULL, 0},
};
EOF
