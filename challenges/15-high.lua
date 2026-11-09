while true do
  local a = beacon()
  if a == nil then
    say("I cannot see the beacon")
  end
  motors(0.1, 0.1)
end
