# Problem:
# https://www.facebook.com/photo.php?fbid=10103916869928063
# Solution:
# https://gist.github.com/nikitaborisov/686e65418469a448157c2734ea6a8da1

import functools

MENU = (
    ('cake', 99),
    ('cupcake', 20),
    ('donut', 10),
    ('muffin', 25),
    ('cookie', 5)
)
PRICE = 1035

@functools.lru_cache(maxsize=None)
def combinations(menu, remaining):
    if not menu:
        if remaining == 0:
            return 1
        else:
            return 0
    cur_price = menu[0][1]
    return sum(combinations(menu[1:],
        remaining - num_cur * cur_price)
        for num_cur in range(remaining // cur_price + 1))

print("{} combinations".format(combinations(MENU, PRICE)))
