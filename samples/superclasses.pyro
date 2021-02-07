class Shape {
    var area;

    def $init(area) {
        self.area = area;
    }

    def print() {
        echo "Shape with area:", self.area;
    }
}

class Circle < Shape {
    var radius;

    def $init(radius) {
        super.$init(3.14 * radius * radius);
        self.radius = radius;
    }

    def print() {
        echo "Circle with area:", self.area;
    }
}

class RedCircle < Circle {
    def print() {
        echo "Red circle with area:", self.area;
    }
}

def $main() {
    var shape = Shape(10);
    shape.print();

    var circle = Circle(4);
    circle.print();

    var red_circle = RedCircle(4);
    red_circle.print();
}
