/* Copyright 2024 Damian Kolaska */
#include "player.h"

#include "config.h"
#include "event.h"
#include "event_system.h"

namespace ascii_combat {

// TODO(GRO4T): Move position and velocity to Vector2D.
Player::Player(const Box& box, const Attributes& attrs, const Direction& facing_direction,
               const Player::Controls& controls, const Map& map)
    : x_(box.x),
      y_(box.y),
      vx_(0),
      vy_(0),
      width_(box.width),
      height_(box.height),
      speed_(attrs.speed),
      jump_force_(attrs.jump_force),
      attack_range_(attrs.attack_range),
      health_(attrs.health),
      facing_direction_(facing_direction),
      is_attacking_(false),
      is_dead_(false),
      controls_(controls),
      map_(map) {
    for (auto anim : Assets::Instance().GetPlayerAnimations()) {
        animations_.emplace_back(anim);
    }
    current_clip_ = &animations_[0][0];
}

const std::vector<std::vector<Clip>>& Player::GetAnimations() const { return animations_; }

Clip* Player::GetCurrentClip() const { return current_clip_; }

int32_t Player::GetX() const { return x_; }

int32_t Player::GetY() const { return y_; }

uint32_t Player::GetWidth() const { return width_; }

int32_t Player::GetHealth() const { return health_; }

const Player::Direction& Player::GetFacingDirection() const { return facing_direction_; }

bool Player::IsDead() const { return health_ <= 0; }

uint32_t Player::GetAttackRange() const { return attack_range_; }

void Player::Draw(Window& window) {
    int draw_x = x_;
    bool is_current_clip_attack =
        current_clip_ == &animations_[3][0] || current_clip_ == &animations_[3][1];
    if (is_current_clip_attack && Direction::kLeft == facing_direction_) {
        draw_x -= 4;
    }
    current_clip_->Draw(window, y_, draw_x);
}

void Player::Update(const Input& input, Player& opponent) {
    if (is_dead_) {
        current_clip_ = &animations_[4][0];
        return;
    }

    auto [dx, dy] = UpdateMovement(input);

    bool is_current_clip_attack =
        current_clip_ == &animations_[3][0] || current_clip_ == &animations_[3][1];
    if (is_attacking_ && is_current_clip_attack && current_clip_->IsFinished()) {
        EventSystem::Instance().CreateEvent(
            std::make_unique<PlayerAttackEvent>(*this, opponent, 1));
        is_attacking_ = false;
        return;
    }

    UpdateCurrentClip();
    Move(dx, dy);
}

void Player::Move(int32_t dx, int32_t dy) {
    // TODO(GRO4T): refactor this
    // TODO(GRO4T): take into account player's width when it collides with the right wall
    if (!is_dead_) {
        if (vy_ == 0 && dy != 0 && is_grounded_) {
            vy_ = dy;
            is_grounded_ = false;
        } else if (!is_grounded_) {
            ++vy_;
        }

        if (dx != 0 && is_grounded_) {  // slower on ground
            if (dx > 0)
                --dx;
            else if (dx < 0)
                ++dx;
        }
        vx_ = dx;

        // collision detection
        bool cOccured = false;
        if (vy_ > 0) {
            int VY = vy_;
            for (int v = 1; v <= VY && !cOccured; ++v) {  // start with vector with length 1
                vy_ = v;
                if (y_ + height_ - 1 + v < kWindowHeight) {  // range checking
                    for (std::size_t c = x_ + 1; c < x_ + width_ - 1 && !cOccured;
                         ++c) {  // check if we can move by this vector
                        if (map_[y_ + height_ + v - 1][c] ==
                            'M') {  // if not, answer is vector 1
                                    // shorter otherwise we continue
                            vy_ = v - 1;
                            is_grounded_ = true;
                            cOccured = true;
                        }
                    }
                } else {
                    vy_ = v - 1;
                    cOccured = true;
                }
            }
        } else if (vy_ < 0) {
            int VY = vy_;
            for (int v = -1; v >= VY && !cOccured; --v) {
                vy_ = v;
                if (y_ + v >= 0) {
                    for (std::size_t c = x_ + 1; c < x_ + width_ - 1 && !cOccured; ++c) {
                        if (map_[y_ + v][c] == 'M') {
                            vy_ = v + 1;
                            cOccured = true;
                        }
                    }
                } else {
                    vy_ = v + 1;
                    cOccured = true;
                }
            }
        } else {
            if (map_[y_ + height_][x_ + 1] == ' ' && map_[y_ + height_][x_ + 2] == ' ')
                is_grounded_ = false;
        }
        // for x Axis it will be little more complicated
        // because we wile be taking into account shapes
        cOccured = false;
        if (vx_ > 0) {
            int VX = vx_;
            for (int v = 1; v <= VX && !cOccured; ++v) {
                vx_ = v;
                if (x_ + width_ + v - 1 < kWindowWidth) {
                    for (int32_t c = x_ + width_ - 1; c >= x_ && !cOccured; --c) {
                        for (int32_t r = y_; r < y_ + (int32_t)height_; ++r) {
                            if (map_[r][c + v] == 'M' &&
                                animations_[0][0].GetFrames()[0][r - y_][c - x_] != ' ') {
                                vx_ = v - 1;
                                cOccured = true;
                            }
                        }
                    }
                } else {
                    vx_ = v - 1;
                    cOccured = true;
                }
            }
        } else if (vx_ < 0) {
            int VX = vx_;
            for (int v = -1; v >= VX && !cOccured; --v) {
                vx_ = v;
                if (x_ + v >= 0) {
                    for (int32_t c = x_; c < x_ + (int32_t)width_ && !cOccured; ++c) {
                        for (int32_t r = y_; r < y_ + (int32_t)height_; ++r) {
                            if (map_[r][c + v] == 'M' &&
                                animations_[0][0].GetFrames()[0][r - y_][c - x_] != ' ') {
                                vx_ = v + 1;
                                cOccured = true;
                            }
                        }
                    }
                } else {
                    vx_ = v + 1;
                    cOccured = true;
                }
            }
        }
        vy_ = std::clamp(vy_, -3, 3);

        x_ += vx_;
        y_ += vy_;
        x_ = std::clamp(x_, 0, (int32_t)(kWindowWidth - width_));
        y_ = std::clamp(y_, 0, (int32_t)(kWindowHeight - height_ - 1));
    }
}

void Player::GotHit(uint32_t damage, const Direction& attacked_from) {
    health_ -= damage;

    if (Direction::kRight == attacked_from) {
        Move(-1, -1);
    } else {
        Move(1, -1);
    }
}

std::pair<int32_t, int32_t> Player::UpdateMovement(const Input& input) {
    bool is_attack_pressed = input.at(controls_.attack) == KeyState::kPressed;
    bool is_jump_pressed = input.at(controls_.jump) == KeyState::kPressed;
    bool is_left_pressed = input.at(controls_.left) == KeyState::kPressed;
    bool is_right_pressed = input.at(controls_.right) == KeyState::kPressed;

    int32_t dx = 0;
    int32_t dy = 0;
    if (is_attack_pressed && is_grounded_) {
        is_attacking_ = true;
    } else if (is_jump_pressed) {
        dy -= jump_force_;
    } else if (is_left_pressed && !is_attacking_) {
        dx -= speed_;
        facing_direction_ = Direction::kLeft;
    } else if (is_right_pressed && !is_attacking_) {
        dx += speed_;
        facing_direction_ = Direction::kRight;
    }

    return {dx, dy};
}

void Player::UpdateCurrentClip() {
    if (is_attacking_) {
        if (Direction::kRight == facing_direction_) {
            current_clip_ = &animations_[3][0];
        } else {
            current_clip_ = &animations_[3][1];
        }
    } else if (is_grounded_ && vx_ != 0) {
        if (Direction::kRight == facing_direction_) {
            current_clip_ = &animations_[1][0];
        } else {
            current_clip_ = &animations_[1][1];
        }
    } else if (!is_grounded_) {
        current_clip_ = &animations_[2][0];  // falling
    } else {
        current_clip_ = &animations_[0][0];  // idle
    }
}

Player PlayerFactory::CreatePlayer(int32_t x, int32_t y, const Player::Direction& facing_direction,
                                   const Player::Controls& controls, const Map& map) {
    Box box{x, y, kPlayerWidth, kPlayerHeight};
    Player::Attributes attrs{kPlayerSpeed, kPlayerJumpForce, kPlayerAttackRange, kPlayerHealth};
    return Player(box, attrs, facing_direction, controls, map);
}

}  // namespace ascii_combat
