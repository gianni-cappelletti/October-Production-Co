function (event, funcs) {
    if (event.type === 'start') {
        var containers = [event.icon, event.settings];
        var closeOtherFileLists = function (e) {
            if (!document.body.contains(event.icon[0])) {
                $(document).off('mousedown', closeOtherFileLists);
                return;
            }
            containers.forEach(function (container) {
                if (!container) {
                    return;
                }
                container.find('.mod-enumerated').each(function () {
                    if (!this.contains(e.target)) {
                        $(this).find('.mod-enumerated-list').hide();
                    }
                });
            });
        };
        $(document).on('mousedown', closeOtherFileLists);
        return;
    }

    if (event.type !== 'change' || event.value < 0.5) {
        return;
    }
    if (event.symbol === 'low_solo') {
        funcs.set_port_value('high_solo', 0);
    } else if (event.symbol === 'high_solo') {
        funcs.set_port_value('low_solo', 0);
    }
}
