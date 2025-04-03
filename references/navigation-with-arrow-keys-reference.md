# left

```python
    if first_char() and not first_line():
        move_cursor_up() # cy -= 1
        cx = char_array_end()
        rx = render_array_end()
        max_visited = rx
    if first_char() and first_line():
        return
    if prev_char is TAB:
        move_cursor_back_TAB_space() # rx
        cx -= 1
        max_visited = rx
    if prev_char is not TAB:
        rx -= 1
        cx -= 1
        max_visited = rx

```

# right

```python
    if last_char() and not last_line():
        move_cursor_down() # cy += 1
        cx = rx = max_visited = 0
    if last_char() and last_line():
        return
    if cur_char is TAB:
        move_cursor_front_TAB_space() # rx
        cx += 1
        max_visited = rx
    if cur_char is not TAB:
        rx += 1
        cx += 1
        max_visited = rx

```

# up

```python
    if first_line():
        return
    
    move_cursor_up() # cy -= 1
    cx = rx_to_cx(cur_line, max_visited) # cur_line = cy
    rx = cx_to_rx(cur_line, cx)

    if cx > char_array_len_cur_line:
        cx = char_array_len_cur_line
        rx = render_array_len_cur_line

```

# down

```python
    if last_line():
        return

    move_cursor_down() # cy -= 1
    cx = rx_to_cx(cur_len, max_visited) # cur_len = cy
    rx = cx_to_rx(cur_len, cx)

    if cx > char_array_len_cur_line:
        cx = char_array_len_cur_line
        rx = render_array_len_cur_line

```

