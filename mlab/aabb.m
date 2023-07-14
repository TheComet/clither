
Ax = [0.798340, 0.031631, -0.288986, 0.274429];
Ay = [-1.840576, -0.185532, -0.060379, 0.040573];

t = linspace(0, 1);
x = Ax(1) + Ax(2)*t + Ax(3)*t.^2 + Ax(4)*t.^3;
y = Ay(1) + Ay(2)*t + Ay(3)*t.^2 + Ay(4)*t.^3;

a = 3*Ax(4);
b = 2*Ax(3);
c = Ax(2);
t1 = (-b + sqrt(b^2 - 4*a*c)) / (2*a);
t2 = (-b - sqrt(b^2 - 4*a*c)) / (2*a);
x1 = Ax(1);
x2 = Ax(1) + Ax(2)*t1 + Ax(3)*t1.^2 + Ax(4)*t1.^3;
x3 = Ax(1) + Ax(2)*t2 + Ax(3)*t2.^2 + Ax(4)*t2.^3;
x4 = Ax(1) + Ax(2) + Ax(3) + Ax(4);
bbx1 = min([x1, x2, x3, x4]);
bbx2 = max([x1, x2, x3, x4]);

a = 3*Ay(4);
b = 2*Ay(3);
c = Ay(2);
t1 = (b + sqrt(b^2 - 4*a*c)) / (2*a);
t2 = (b - sqrt(b^2 - 4*a*c)) / (2*a);
y1 = Ay(1);
y2 = Ay(1) + Ay(2)*t1 + Ay(3)*t1.^2 + Ay(4)*t1.^3;
y3 = Ay(1) + Ay(2)*t2 + Ay(3)*t2.^2 + Ay(4)*t2.^3;
y4 = Ay(1) + Ay(2) + Ay(3) + Ay(4);
bby1 = min([y1, y2, y4]);
bby2 = max([y1, y2, y4]);

hold off
plot(x, y);
hold on
plot([bbx1, bbx1], [bby1, bby2]);
plot([bbx2, bbx2], [bby1, bby2]);
plot([bbx1, bbx2], [bby1, bby1]);
plot([bbx1, bbx2], [bby2, bby2]);

