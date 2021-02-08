def simple_loop() {
    var i = 0;
    while i < 5 {
        echo i;
        i = i + 1;
    }
}

def complex_loop() {
    var i = 0;
    while i < 10 {
        i = i + 1;
        if i == 3 {
            continue;
        }
        if i == 6 {
            break;
        }
        echo i;
    }
}

def nested_loops() {
    var x = 0;
    while x <= 2 {
        var y = 0;
        x = x + 1;
        while y <= 2 {
            y = y + 1;
            echo x, y;
        }
    }
}

def $main() {
    simple_loop();
    echo;
    complex_loop();
    echo;
    nested_loops();
}
