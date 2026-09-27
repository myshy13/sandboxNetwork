# Bugs

1. F3 goes above the pause menu (simple fix)

2. Holding Space while inWater re-fires the swim impulse every frame (Player::Update, player.cpp) — the ground jump self-corrects because onGround flips false right after, but inWater doesn't, so velocity.y keeps getting reset to the swim impulse instead of a single kick.
