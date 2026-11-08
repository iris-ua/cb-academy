while true do
  local e = ir(1) - ir(2)
  motors(0.1 + 0.05 * e, 0.1 - 0.05 * e)
end
