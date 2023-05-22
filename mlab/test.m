a0 = 0.065674;
a1 = -0.052603;
a2 = 0.060538;
a3 = -0.011841;

b0 = -0.1497;
b1 = -0.063364;
b2 = -0.041695;
b3 = 0.023760;

t = linspace(0, 1, 20);

hold off
plot(a0 + a1*t + a2*t.^2 + a3*t.^3, b0 + b1*t + b2*t.^2 + b3*t.^3);
hold on

for t = linspace(0, 1, 20)
  x = a0 + a1*t + a2*t^2 + a3*t^3;
  y = b0 + b1*t + b2*t^2 + b3*t^3;
  dx = a1 + 2*a2*t + 3*a3*t^2;
  dy = b1 + 2*b2*t + 3*b3*t^2;
  plot([x, x+dy], [y, y-dx]);
end

