while ground() < 0 do
  local d = 1 / math.max(ir(0), 0.05)
  local v = math.min(0.15, math.max(0.02, 0.12 * (d - 0.3)))
  local e = (0 - compass() + 180) % 360 - 180
  motors(v - 0.003 * e, v + 0.003 * e)
end
