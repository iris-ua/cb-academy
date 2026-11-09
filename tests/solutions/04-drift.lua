while true do
  local e = (0 - compass() + 180) % 360 - 180
  motors(0.12 - 0.003 * e, 0.12 + 0.003 * e)
end
