function (event, funcs) {
    if (event.type !== 'change' || event.value < 0.5) {
        return;
    }
    if (event.symbol === 'low_solo') {
        funcs.set_port_value('high_solo', 0);
    } else if (event.symbol === 'high_solo') {
        funcs.set_port_value('low_solo', 0);
    }
}
