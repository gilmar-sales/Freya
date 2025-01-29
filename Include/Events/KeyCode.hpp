#pragma once

namespace FREYA_NAMESPACE
{
    enum class KeyCode
    {
        Unknown = 0,

        A = 4,
        B = 5,
        C = 6,
        D = 7,
        E = 8,
        F = 9,
        G = 10,
        H = 11,
        I = 12,
        J = 13,
        K = 14,
        L = 15,
        M = 16,
        N = 17,
        O = 18,
        P = 19,
        Q = 20,
        R = 21,
        S = 22,
        T = 23,
        U = 24,
        V = 25,
        W = 26,
        X = 27,
        Y = 28,
        Z = 29,

        Num1 = 30,
        Num2 = 31,
        Num3 = 32,
        Num4 = 33,
        Num5 = 34,
        Num6 = 35,
        Num7 = 36,
        Num8 = 37,
        Num9 = 38,
        Num0 = 39,

        Return = 40,
        Escape = 41,
        Backspace = 42,
        Tab = 43,
        Space = 44,

        Minus = 45,
        Equals = 46,
        LeftBracket = 47,
        RightBracket = 48,
        Backslash = 49,
        NonUsHash = 50,
        Semicolon = 51,
        Apostrophe = 52,
        Grave = 53,
        Comma = 54,
        Period = 55,
        Slash = 56,

        CapsLock = 57,

        F1 = 58,
        F2 = 59,
        F3 = 60,
        F4 = 61,
        F5 = 62,
        F6 = 63,
        F7 = 64,
        F8 = 65,
        F9 = 66,
        F10 = 67,
        F11 = 68,
        F12 = 69,

        PrintScreen = 70,
        ScrollLock = 71,
        Pause = 72,
        Insert = 73,
        Home = 74,
        PageUp = 75,
        Delete = 76,
        End = 77,
        PageDown = 78,
        Right = 79,
        Left = 80,
        Down = 81,
        Up = 82,

        NumLockClear = 83,
        KpDivide = 84,
        KpMultiply = 85,
        KpMinus = 86,
        KpPlus = 87,
        KpEnter = 88,
        Kp1 = 89,
        Kp2 = 90,
        Kp3 = 91,
        Kp4 = 92,
        Kp5 = 93,
        Kp6 = 94,
        Kp7 = 95,
        Kp8 = 96,
        Kp9 = 97,
        Kp0 = 98,
        KpPeriod = 99,

        NonUsBackslash = 100,
        Application = 101,
        Power = 102,
        KpEquals = 103,
        F13 = 104,
        F14 = 105,
        F15 = 106,
        F16 = 107,
        F17 = 108,
        F18 = 109,
        F19 = 110,
        F20 = 111,
        F21 = 112,
        F22 = 113,
        F23 = 114,
        F24 = 115,
        Execute = 116,
        Help = 117,
        Menu = 118,
        Select = 119,
        Stop = 120,
        Again = 121,
        Undo = 122,
        Cut = 123,
        Copy = 124,
        Paste = 125,
        Find = 126,
        Mute = 127,
        VolumeUp = 128,
        VolumeDown = 129,
        KpComma = 133,
        KpEqualsAs400 = 134,

        International1 = 135,
        International2 = 136,
        International3 = 137,
        International4 = 138,
        International5 = 139,
        International6 = 140,
        International7 = 141,
        International8 = 142,
        International9 = 143,
        Lang1 = 144,
        Lang2 = 145,
        Lang3 = 146,
        Lang4 = 147,
        Lang5 = 148,
        Lang6 = 149,
        Lang7 = 150,
        Lang8 = 151,
        Lang9 = 152,

        AltErase = 153,
        SysReq = 154,
        Cancel = 155,
        Clear = 156,
        Prior = 157,
        Return2 = 158,
        Separator = 159,
        Out = 160,
        Oper = 161,
        ClearAgain = 162,
        CrSel = 163,
        ExSel = 164,

        Kp00 = 176,
        Kp000 = 177,
        ThousandsSeparator = 178,
        DecimalSeparator = 179,
        CurrencyUnit = 180,
        CurrencySubunit = 181,
        KpLeftParen = 182,
        KpRightParen = 183,
        KpLeftBrace = 184,
        KpRightBrace = 185,
        KpTab = 186,
        KpBackspace = 187,
        KpA = 188,
        KpB = 189,
        KpC = 190,
        KpD = 191,
        KpE = 192,
        KpF = 193,
        KpXor = 194,
        KpPower = 195,
        KpPercent = 196,
        KpLess = 197,
        KpGreater = 198,
        KpAmpersand = 199,
        KpDblAmpersand = 200,
        KpVerticalBar = 201,
        KpDblVerticalBar = 202,
        KpColon = 203,
        KpHash = 204,
        KpSpace = 205,
        KpAt = 206,
        KpExclam = 207,
        KpMemStore = 208,
        KpMemRecall = 209,
        KpMemClear = 210,
        KpMemAdd = 211,
        KpMemSubtract = 212,
        KpMemMultiply = 213,
        KpMemDivide = 214,
        KpPlusMinus = 215,
        KpClear = 216,
        KpClearEntry = 217,
        KpBinary = 218,
        KpOctal = 219,
        KpDecimal = 220,
        KpHexadecimal = 221,

        LCtrl = 224,
        LShift = 225,
        LAlt = 226,
        LGui = 227,
        RCtrl = 228,
        RShift = 229,
        RAlt = 230,
        RGui = 231,

        Mode = 257,
        Sleep = 258,
        Wake = 259,

        CHANNEL_INCREMENT = 260, /**< Channel Increment */
        CHANNEL_DECREMENT = 261, /**< Channel Decrement */

        MEDIA_PLAY = 262, /**< Play */
        MEDIA_PAUSE = 263, /**< Pause */
        MEDIA_RECORD = 264, /**< Record */
        MEDIA_FAST_FORWARD = 265, /**< Fast Forward */
        MEDIA_REWIND = 266, /**< Rewind */
        MEDIA_NEXT_TRACK = 267, /**< Next Track */
        MEDIA_PREVIOUS_TRACK = 268, /**< Previous Track */
        MEDIA_STOP = 269, /**< Stop */
        MEDIA_EJECT = 270, /**< Eject */
        MEDIA_PLAY_PAUSE = 271, /**< Play / Pause */
        MEDIA_SELECT = 272, /* Media Select */

        AC_NEW = 273, /**< AC New */
        AC_OPEN = 274, /**< AC Open */
        AC_CLOSE = 275, /**< AC Close */
        AC_EXIT = 276, /**< AC Exit */
        AC_SAVE = 277, /**< AC Save */
        AC_PRINT = 278, /**< AC Print */
        AC_PROPERTIES = 279, /**< AC Properties */

        AC_SEARCH = 280, /**< AC Search */
        AC_HOME = 281, /**< AC Home */
        AC_BACK = 282, /**< AC Back */
        AC_FORWARD = 283, /**< AC Forward */
        AC_STOP = 284, /**< AC Stop */
        AC_REFRESH = 285, /**< AC Refresh */
        AC_BOOKMARKS = 286, /**< AC Bookmarks */

        SOFTLEFT = 287, /**< Usually situated below the display on phones and
                                           used as a multi-function feature key for selecting
                                           a software defined function shown on the bottom left
                                           of the display. */
        SOFTRIGHT = 288, /**< Usually situated below the display on phones and
                                           used as a multi-function feature key for selecting
                                           a software defined function shown on the bottom right
                                           of the display. */
        CALL = 289, /**< Used for accepting phone calls. */
        ENDCALL = 290, /**< Used for rejecting phone calls. */

        Reserved = 400,

        SdlNumScancodes = 512,
    };
} // namespace FREYA_NAMESPACE
