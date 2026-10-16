function (event, funcs) {
    var qualityFullThreshold = 50;

    var updateQualityLever = function (symbol, value) {
        event.icon.find('[opc-quality-symbol="' + symbol + '"]')
            .toggleClass('opc-full', value >= qualityFullThreshold)
            .data('value', value);
    };

    if (event.type === 'start') {
        event.ports.forEach(function (port) {
            updateQualityLever(port.symbol, port.value);
        });

        event.icon.find('[opc-quality-symbol]').on('click', function (e) {
            var lever = $(this);
            var symbol = lever.attr('opc-quality-symbol');
            var value = lever.data('value') >= qualityFullThreshold ? 0 : 100;
            funcs.set_port_value(symbol, value);
            updateQualityLever(symbol, value);
            e.stopPropagation();
        });

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

    if (event.type !== 'change' || event.symbol === undefined) {
        return;
    }

    updateQualityLever(event.symbol, event.value);

    if (event.value < 0.5) {
        return;
    }
    if (event.symbol === 'low_solo') {
        funcs.set_port_value('high_solo', 0);
    } else if (event.symbol === 'high_solo') {
        funcs.set_port_value('low_solo', 0);
    }
}
