while true do
  local a = beacon()
  if a then
    local k = math.max(-0.05, math.min(0.05, a / 300))
    motors(0.1 - k, 0.1 + k)
  else
    motors(-0.05, 0.05)
  end
end
