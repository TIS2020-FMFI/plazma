def validate_float(value):
    try:
        float(value)
        return True
    except ValueError:
        # return False
        return True


def validate_address(value):
    # TODO
    # ci je int
    # rozmedzie ASI od 1 po 32, mozno netreba
    try:
        int(value)
        return True
    except ValueError:
        return False


def validate_port_length(value):
    # ci je float
    # mozno este validacia nejaka po konverzii... dlzka kabla = 25 , vzorec na konverziu asi bude: 25 / 300 000
    return False


def validate_velocity_factor(value):
    # ci je float
    # ci je v rozmedzi 0 az 1
    return False


def validate_start_stop(start, stop):
    # ci je start mensie ako stop
    # overit mozno ci je tam nejake rozmedzie, ktore pristroj nevezme
    # return False
    return True


def validate_points(value):
    # ci je to int
    # ci je medzi 1 az 1601 vratane
    # return False
    return True
