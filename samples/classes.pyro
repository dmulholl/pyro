class Person {
    var name;
    var age;
    var is_alive = true;

    def $init(name, age) {
        self.name = name;
        self.age = age;
    }

    def print() {
        echo self.name, self.age, self.is_alive;
    }
}

def $main() {
    var dave = Person("Dave", 21);
    dave.print();
    dave.is_alive = false;
    dave.print();
}
