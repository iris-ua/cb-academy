local function leg(h)
  turn_to(h)
  while ir(0) < 1 do
    local e = (h - compass() + 180) % 360 - 180
    motors(0.12 - 0.003 * e, 0.12 + 0.003 * e)
  end
end
for _, h in ipairs({ 0, 90, 180, -90 }) do leg(h) end
