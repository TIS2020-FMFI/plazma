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


def validate_port_length(value):
    # TODO overiť dĺžku - rozmedzie

    try:
        if 0 <= float(value) <= 1:
            return True
    except ValueError:
        return False


def validate_velocity_factor(value):
    try:
        if 0 <= float(value) <= 1:
            return True
    except ValueError:
        return False


def validate_start_stop(start, stop):
    # TODO - zistit rozmedzie start,stop frekvencie
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
