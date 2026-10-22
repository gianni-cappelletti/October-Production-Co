function (event, funcs) {
    var emptySlotText = '-- none --';

    var slotFileParameters = { nam1_on: 'nam1Model', nam2_on: 'nam2Model' };

    var updateSlotLcd = function (symbol, value) {
        var parameter = slotFileParameters[symbol];
        if (parameter === undefined) {
            return;
        }
        event.icon.find('.opc-file[mod-parameter-uri$="#' + parameter + '"]')
            .toggleClass('opc-dim', value < 0.5);
    };

    var fileListsIn = function (container) {
        return container.find('[mod-widget="custom-select-path"]').map(function () {
            var widget = $(this);
            return widget.is('.mod-enumerated-list') ? this : widget.find('.mod-enumerated-list').get(0);
        });
    };

    var containers = function () {
        return [event.icon, event.settings].filter(function (container) {
            return container;
        });
    };

    var addNoneRows = function () {
        containers().forEach(function (container) {
            fileListsIn(container).each(function () {
                var list = $(this);
                if (list.children('.opc-file-none').length === 0) {
                    list.prepend($('<div class="opc-file-none">None</div>'));
                }
            });
        });
    };

    var showSlotFile = function (uri, path) {
        var isEmpty = !path;
        containers().forEach(function (container) {
            container.find('[mod-parameter-uri="' + uri + '"] .opc-file-none').toggleClass('selected', isEmpty);
        });
        if (isEmpty) {
            event.icon.find('.mod-enumerated-selected[mod-parameter-uri="' + uri + '"]').text(emptySlotText);
        }
    };

    if (event.type === 'start') {
        addNoneRows();
        event.ports.forEach(function (port) {
            updateSlotLcd(port.symbol, port.value);
        });
        event.parameters.forEach(function (parameter) {
            showSlotFile(parameter.uri, parameter.value);
        });

        containers().forEach(function (container) {
            container.on('click', '.opc-file-none', function () {
                var uri = $(this).closest('[mod-parameter-uri]').attr('mod-parameter-uri');
                funcs.patch_set(uri, 'p', '');
                showSlotFile(uri, '');
            });
        });

        var onDocumentMouseDown = function (e) {
            if (!document.body.contains(event.icon[0])) {
                $(document).off('mousedown', onDocumentMouseDown);
                return;
            }
            addNoneRows();
            event.icon.find('.opc-file').each(function () {
                $(this).toggleClass('opc-active', this.contains(e.target));
            });
            containers().forEach(function (container) {
                container.find('.mod-enumerated').each(function () {
                    if (!this.contains(e.target)) {
                        $(this).find('.mod-enumerated-list').hide();
                    }
                });
            });
        };
        $(document).on('mousedown', onDocumentMouseDown);
        return;
    }

    if (event.type !== 'change') {
        return;
    }

    if (event.uri !== undefined) {
        showSlotFile(event.uri, event.value);
        return;
    }

    if (event.symbol === undefined) {
        return;
    }

    updateSlotLcd(event.symbol, event.value);

    if (event.value < 0.5) {
        return;
    }
    if (event.symbol === 'low_solo') {
        funcs.set_port_value('high_solo', 0);
    } else if (event.symbol === 'high_solo') {
        funcs.set_port_value('low_solo', 0);
    }
}
