while true do
  local a = beacon()
  motors(0.1 - 0.003 * a, 0.1 + 0.003 * a)
end
