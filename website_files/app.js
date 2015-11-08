/// <reference path="website_files/classicQ.ts"/>
window.onload = function () {
    InitializeSortableList();
    $('#cfg_button').on('click', OnGenerateConfigClicked);
};
function OnGenerateConfigClicked() {
    var bindings = [];
    $('#sortable').children().each(function (index) {
        var currentText = $(this).text();
        var configProperty = new ConfigPropery(index, currentText);
        bindings.push(configProperty);
    });
    var config = new Config(bindings, $('.toggle-on').hasClass('active'), $('#nickname').val());
    var blob = new Blob([config.GetConfig()], { type: "text/plain;charset=utf-8" });
    saveAs(blob, "initial_keybinds.cfg", true);
    return false;
}
function InitializeSortableList() {
    $('#sortable0').text(CommandEnum.MOVESFORWARD.str);
    $('#sortable1').text(CommandEnum.MOVESBACK.str);
    $('#sortable2').text(CommandEnum.MOVESLEFT.str);
    $('#sortable3').text(CommandEnum.MOVESRIGHT.str);
    $('#sortable4').text(CommandEnum.JUMP.str);
    $('#sortable5').text(CommandEnum.ROCKETLAUNCHER.str);
    $('#sortable6').text(CommandEnum.THUNDERBOLT.str);
    $('#sortable7').text(CommandEnum.GRENADELAUNCHER.str);
    $('#sortable8').text(CommandEnum.AXE.str);
    $('#sortable9').text(CommandEnum.SHOTGUN.str);
    $('#sortable10').text(CommandEnum.NAILGUN.str);
}
//# sourceMappingURL=app.js.map