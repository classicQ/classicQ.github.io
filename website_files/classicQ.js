var Config = (function () {
    function Config(bindings, invertMouse, nickName) {
        this.configHeader = '\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\r\n\/\/                                                                                                \/\/\r\n\/\/  These initial settings will be executed only as long as there is no new default.cfg in        \/\/\r\n\/\/  \\Fodquake\\fodquake\\configs\\ folder.                                                           \/\/\r\n\/\/                                                                                                \/\/ \r\n\/\/  To override, simply type \"cfg_save\" and then \"cfg_save\" again to confirm.                     \/\/\r\n\/\/  This will create a new \\Fodquake\\fodquake\\configs\\default.cfg file with the new settings.     \/\/\r\n\/\/                                                                                                \/\/\r\n\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\r\n\r\n';
        this.nickName = nickName.length ? nickName : 'unnamed';
        this.invertMouse = invertMouse;
        this.bindings = [];
        this.bindings.push(new ConfigPropery(-1, this.nickName));
        this.bindings.push(new ConfigPropery(-2, this.nickName.length >= 3 ? this.nickName.substr(0, 3) : this.nickName.substr(0, this.nickName.length)));
        this.bindings.push(new ConfigPropery(-3, this.invertMouse ? '0.022' : '-0.022'));
        for (var binding in bindings) {
            this.bindings.push(bindings[binding]);
        }
        this.invertMouse = invertMouse;
        this.nickName = nickName;
    }
    Config.prototype.GetConfig = function () {
        var result = this.configHeader;
        for (var configProperty in this.bindings) {
            result += this.bindings[configProperty].ToString() + '\r\n';
        }
        return result;
    };
    ;
    return Config;
})();
var ConfigPropery = (function () {
    function ConfigPropery(index, bindingValue) {
        this.charsToFirstQuota = 20;
        this.binding = this.ResolveCommand(index);
        this.bindingValue = this.ResolveValue(bindingValue, index);
        this.indent = this.charsToFirstQuota - this.binding.length;
    }
    ConfigPropery.prototype.ResolveValue = function (bindingValue, index) {
        if (index < 0) {
            return '"' + bindingValue + '"';
        }
        if (bindingValue == CommandEnum.MOVESFORWARD.str) {
            return '"+forward"';
        }
        else if (bindingValue == CommandEnum.MOVESBACK.str) {
            return '"+back"';
        }
        else if (bindingValue == CommandEnum.MOVESLEFT.str) {
            return '"+moveleft"';
        }
        else if (bindingValue == CommandEnum.MOVESRIGHT.str) {
            return '"+moveright"';
        }
        else if (bindingValue == CommandEnum.JUMP.str) {
            return '"+jump"';
        }
        else if (bindingValue == CommandEnum.ROCKETLAUNCHER.str) {
            return '"+weapon_rocket"';
        }
        else if (bindingValue == CommandEnum.THUNDERBOLT.str) {
            return '"+weapon_thunderbolt"';
        }
        else if (bindingValue == CommandEnum.GRENADELAUNCHER.str) {
            return '"+weapon_grenade"';
        }
        else if (bindingValue == CommandEnum.AXE.str) {
            return '"+weapon_axe"';
        }
        else if (bindingValue == CommandEnum.SHOTGUN.str) {
            return '"+weapon_shotgun"';
        }
        else if (bindingValue == CommandEnum.NAILGUN.str) {
            return '"+weapon_nailgun"';
        }
        return '';
    };
    ConfigPropery.prototype.ResolveCommand = function (index) {
        if (index == -1) {
            return 'name';
        }
        else if (index == -2) {
            return 'set  name_short';
        }
        else if (index == -3) {
            return 'm_pitch';
        }
        else if (index == 0) {
            return 'bind  w';
        }
        else if (index == 1) {
            return 'bind  s';
        }
        else if (index == 2) {
            return 'bind  a';
        }
        else if (index == 3) {
            return 'bind  d';
        }
        else if (index == 4) {
            return 'bind  SPACE';
        }
        else if (index == 5) {
            return 'bind  MOUSE1';
        }
        else if (index == 6) {
            return 'bind  MOUSE2';
        }
        else if (index == 7) {
            return 'bind  MOUSE3';
        }
        else if (index == 8) {
            return 'bind  q';
        }
        else if (index == 9) {
            return 'bind  e';
        }
        else if (index == 10) {
            return 'bind  SHIFT';
        }
        return '';
    };
    ConfigPropery.prototype.ToString = function () {
        var result = this.binding;
        for (var i = 0; i < this.indent; i++) {
            result += ' ';
        }
        result += this.bindingValue;
        return result;
    };
    return ConfigPropery;
})();
var CommandEnum = (function () {
    function CommandEnum() {
    }
    CommandEnum.MOVESFORWARD = { str: "moves forward", id: 0 };
    CommandEnum.MOVESBACK = { str: "moves back", id: 1 };
    CommandEnum.MOVESLEFT = { str: "moves left", id: 2 };
    CommandEnum.MOVESRIGHT = { str: "moves right", id: 3 };
    CommandEnum.JUMP = { str: "jump", id: 4 };
    CommandEnum.ROCKETLAUNCHER = { str: "(primary attack) Rocket Launcher", id: 5 };
    CommandEnum.THUNDERBOLT = { str: "(secondary attack) Thunderbolt", id: 6 };
    CommandEnum.GRENADELAUNCHER = { str: "(tertiary attack) Grenade Launcher", id: 7 };
    CommandEnum.AXE = { str: "Axe", id: 8 };
    CommandEnum.SHOTGUN = { str: "Shotgun", id: 9 };
    CommandEnum.NAILGUN = { str: "Nailgun", id: 10 };
    return CommandEnum;
})();
//# sourceMappingURL=classicQ.js.map