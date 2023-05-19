clear all;

% Generate some random data
t = linspace(0, 1, 50);
p = sin(pi*2/3*t+0.3) + cos(pi*2*t) + rand(1, 50)*0.2;
p = ones(1, 50);

% Standard regression for reference
T = [ones(1,50); t; t.^2; t.^3]';
c = (T'*T)^-1 * T' * p';
fit1 = c(1) + c(2)*t + c(3)*t.^2 + c(4)*t.^3;

% Calculate f(t) -- polynomial that passes through all constrained points
% y = m*x + q
% y - m*x = q
% y1 = m*x1 + q
% y2 = m*x2 + q
% y1 - m*x1 = y2 - m*x2
% y1-y2 = m*x1 - m*x2
% (y1-y2)/(x1-x2) = m
m = (p(1)-p(end)) / (t(1)-t(end));
q = p(1) - m*t(1);
f = m*t + q;

% Constrained regression
% https://stats.stackexchange.com/questions/50447/perform-linear-regression-but-force-solution-to-go-through-some-particular-data
% y = f(t) + (t-t1)(t-t2)(c0 + c1*t + c2*t^2 + c3*t^3)
% r(t) = (t-t1)(t-t2)
% y - f(t) = c0*r(t) + c1*r(t)*t + c2*r(t)*t^2 + c3*r(t)*t^3
r = (t-t(1)).*(t-t(end));
Y = p - f;
T = [r; r.*t; r.*t.^2; r.*t.^3]';
c = (T'*T)^-1 * T' * Y';
fit2 = (c(1)*r + c(2)*r.*t + c(3)*r.*t.^2 + c(4)*r.*t.^3) + f;

hold off
scatter(t, p);
hold on
plot(t, fit1, 'r--');
plot(t, fit2, 'r');

