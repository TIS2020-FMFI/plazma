def validate_float(value):
    try:
        float(value)
        return True
    except ValueError:
        return False


def validate_address(value):
    try:
        if int(value) in range(1, 33):
            return True
    except ValueError:
        return False


def validate_velocity_factor(value):
    try:
        if 0 <= float(value) <= 10:
            return True
    except ValueError:
        return False


def validate_port_length(value):
    try:
        if 0 <= float(value) <= 1000000000000:
            return True
    except ValueError:
        return False


def validate_start_stop(start, stop, freq_unit):
    if freq_unit == 1:
        if float(stop) > 3 or float(start) > 3:
            return False
    if freq_unit == 0:
        if float(stop) > 3000 or float(start) > 3000:
            return False
    if float(start) < 0 or float(stop) < 0:
        return False
    if float(start) < float(stop):
        return True
    return False


def validate_points(value):
    try:
        if int(value) in range(1, 1602):
            return True
    except ValueError:
        return False
