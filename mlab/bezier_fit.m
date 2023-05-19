close all; clear all;

% TODO https://stats.stackexchange.com/questions/50447/perform-linear-regression-but-force-solution-to-go-through-some-particular-data
% https://stackoverflow.com/questions/15191088/how-to-do-a-polynomial-fit-with-fixed-points

t = linspace(0, 1, 50);
p = sin(pi/3*2*t+0.3) + rand(1, 50)*0.1;

T = [ones(1,50); t; t.^2; t.^3]';
c = (T'*T)^-1 * T' * p';
fit1=c(1) + c(2)*t + c(3)*t.^2 + c(4)*t.^3;



plot(t, p, t, fit1);

