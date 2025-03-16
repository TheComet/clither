a = [3; 4];
n = [9; 26];
p = [2; 6];
n = n ./ norm(n);

t = linspace(-10, 10);
x = a + t.*n;

r = norm(cross([p - a; 0], [n; 0])) / norm(n);

nn = [n(2); -n(1)];
ax=a(1); ay=a(2);
nx=n(1); ny=n(2);
px=p(1); py=p(2);
pp = [
    px - 2*((px-ax)*ny - (py-ay)*nx) * ny;
    py - 2*((px-ax)*ny + (py-ay)*nx) * nx*-1;
];

diffx = px - ax;
diffy = py - ay;
dotx = diffx*ny - diffy*nx;
doty = diffx*ny + diffy*nx;
%pp = [ px - 2*dotx*ny; py - 2*doty*nx ];

hold off
plot(x(1,:), x(2,:));
hold on
plot([a(1), a(1)+n(1)], [a(2), a(2)+n(2)]);
plot([a(1), a(1)+nn(1)], [a(2), a(2)+nn(2)]);
plot([p(1), pp(1)], [p(2), pp(2)]);
daspect([1 1 1]);
scatter([a(1)], [a(2)]);
scatter([p(1)], [p(2)]);
scatter([pp(1)], [pp(2)]);
rectangle('Position', [p(1)-r, p(2)-r, 2*r, 2*r], 'Curvature', [1, 1]);

