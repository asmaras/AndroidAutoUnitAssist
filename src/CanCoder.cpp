#include "CanCoder.h"
#include <sstream>
#include <iomanip>

bool CanCoder::Decode(uint32_t inIdentifier, uint8_t inDataLengthCode, uint8_t* inData) {
    _identifier = (Identifier)inIdentifier;

    if (inDataLengthCode > 8) {
        return false;
    }

    switch ((Identifier)inIdentifier) {
    case Identifier::frontPassengerSideDoorStatus:
        if (inDataLengthCode >= 4) {
            // The open status is in bit 0 of byte 3
            _frontPassengerSideDoorStatus.open = (inData[3] & 0x01) != 0x00;
            // The lock status is in bits 0 and 1 of byte 0
            _frontPassengerSideDoorStatus.locked = (inData[0] & 0x03) != 0x01;
            return true;
        }
        break;
    case Identifier::rearPassengerSideDoorStatus:
        if (inDataLengthCode >= 4) {
            // The open status is in bit 0 of byte 3
            _rearPassengerSideDoorStatus.open = (inData[3] & 0x01) != 0x00;
            // The lock status is in bits 0 and 1 of byte 0
            _rearPassengerSideDoorStatus.locked = (inData[0] & 0x03) != 0x01;
            return true;
        }
        break;
    case Identifier::frontDriverSideDoorStatus:
        if (inDataLengthCode >= 4) {
            // The open status is in bit 0 of byte 3
            _frontDriverSideDoorStatus.open = (inData[3] & 0x01) != 0x00;
            // The lock status is in bits 0 and 1 of byte 0
            _frontDriverSideDoorStatus.locked = (inData[0] & 0x03) != 0x01;
            return true;
        }
        break;
    case Identifier::rearDriverSideDoorStatus:
        if (inDataLengthCode >= 4) {
            // The open status is in bit 0 of byte 3
            _rearDriverSideDoorStatus.open = (inData[3] & 0x01) != 0x00;
            // The lock status is in bits 0 and 1 of byte 0
            _rearDriverSideDoorStatus.locked = (inData[0] & 0x03) != 0x01;
            return true;
        }
        break;
    case Identifier::mirrorFoldStatus:
        if (inDataLengthCode >= 1) {
            // Byte 0 will be F7 when the mirrors are being folded
            _mirrorFoldStatus.folded = inData[0] == 0xF7;
            return true;
        }
        break;
    case Identifier::ignitionAndKeyLocation:
        if (inDataLengthCode >= 4) {
            // When the key is outside bytes 1 and 3 will be 0E/04 or 01/06 (door handle button is pushed)
            _ignitionAndKeyLocation.keyIsOutside =
                (inData[1] == 0x0E && inData[3] == 0x04) ||
                (inData[1] == 0x01 && inData[3] == 0x06);
            return true;
        }
        break;
    case Identifier::vehicleSpeed:
        if (inDataLengthCode >= 2) {
            // The vehicle speed is in the first 12 bits of bytes 0 and 1
            // The unit is 0.1 km/h
            _vehicleSpeed.speed = inData[0] + ((inData[1] & 0x0F) << 8);
            return true;
        }
        break;
    case Identifier::iDriveControler:
        if (inDataLengthCode >= 4) {
            // The stick direction is encoded in byte 0
            // Each direction has a value rather than using seperate bits
            _iDriveController.stickUp = (inData[0] & 0x0f) == 0x00;
            _iDriveController.stickRight = (inData[0] & 0x0f) == 0x02;
            _iDriveController.stickDown = (inData[0] & 0x0f) == 0x04;
            _iDriveController.stickLeft = (inData[0] & 0x0f) == 0x06;
            // The buttons are encoded in bits 0, 2 and 4 of byte 1
            _iDriveController.stickPush = (inData[1] & 0x01) != 0;
            _iDriveController.homeButton = (inData[1] & 0x04) != 0;
            _iDriveController.menuButton = (inData[1] & 0x10) != 0;
            // The dial value is encode in bytes 2 and 3
            _iDriveController.dialValue = inData[2] + (inData[3] << 8);
            _iDriveController.additionalData.dataLengthCode = inDataLengthCode;
            for (int index = 4; index < inDataLengthCode; index++) {
                _iDriveController.additionalData.uncodedData[index - 4] = inData[index];
            }
            return true;
        }
        break;
    case Identifier::gearShifterPosition:
        if (inDataLengthCode >= 1) {
            // The automatic gear shifter position is encoded in bits 0-3 of byte 0
            // Bits 0-3 respectively represent P-R-N-D
            // Bits 4-7 are an inverted version of bits 0-3
            if ((inData[0] & 0x01) != 0)
            {
                _gearShifterPosition.position = 'P';
            }
            else if ((inData[0] & 0x02) != 0)
            {
                _gearShifterPosition.position = 'R';
            }
            else if ((inData[0] & 0x04) != 0)
            {
                _gearShifterPosition.position = 'N';
            }
            else if ((inData[0] & 0x08) != 0)
            {
                _gearShifterPosition.position = 'D';
            }
            _gearShifterPosition.additionalData.dataLengthCode = inDataLengthCode;
            for (int index = 1; index < inDataLengthCode; index++) {
                _gearShifterPosition.additionalData.uncodedData[index - 1] = inData[index];
            }
            return true;
        }
        break;
    case Identifier::remoteControlAndDoorHandleInput:
        if (inDataLengthCode >= 3) {
            // Bits 0 and 1 of byte 1 are zero when the input comes from the remote control
            // They are 3 when it comes from the door handle
            bool comingFromRemoteControl = (inData[1] & 0x03) == 0;
            // Bit 0 of byte 2 represents the unlock button
            bool unlockButton = (inData[2] & 0x01) != 0;
            if (comingFromRemoteControl) {
                _remoteControlAndDoorHandleInput.remoteControlUnlockButton = unlockButton;
            }
            else {
                _remoteControlAndDoorHandleInput.doorHandleUnlockButton = unlockButton;
            }
            // Bit 2 of byte 2 represents the lock button
            bool lockButton = (inData[2] & 0x04) != 0;
            if (comingFromRemoteControl) {
                _remoteControlAndDoorHandleInput.remoteControlLockButton = lockButton;
            }
            else {
                _remoteControlAndDoorHandleInput.doorHandleLockButton = lockButton;
            }
            return true;
        }
        break;
    case Identifier::windowRoofAndMirrorControl:
        if (inDataLengthCode >= 4) {
            // Closing action for the windows and roof is encode with 0x1b in bytes 0 and 2
            // Byte 3 should be 0x52
            _windowRoofAndMirrorControl.closeWindowsAndRoof = inData[0] == 0x1b && inData[2] == 0x1b && inData[3] == 0x52;
            // Folding action for the mirrors is encode with 0x1b in byte 1
            // Byte 3 should be 0x52
            _windowRoofAndMirrorControl.foldMirrors = inData[1] == 0x1b && inData[3] == 0x52;
            _windowRoofAndMirrorControl.additionalData.dataLengthCode = inDataLengthCode;
            for (int index = 4; index < inDataLengthCode; index++) {
                _windowRoofAndMirrorControl.additionalData.uncodedData[index - 4] = inData[index];
            }
            return true;
        }
        break;
    case Identifier::doorLockControl:
        if (inDataLengthCode >= 4) {
            // Door locking action is encoded in bytes 0-3
            _doorLockControl.lockDoors = inData[0] == 0x33 && inData[1] == 0x33 && inData[2] == 0x38 && inData[3] == 0x00;
            _doorLockControl.additionalData.dataLengthCode = inDataLengthCode;
            for (int index = 4; index < inDataLengthCode; index++) {
                _doorLockControl.additionalData.uncodedData[index - 4] = inData[index];
            }
            return true;
        }
        break;
    case Identifier::dateTime:
    case Identifier::setDateTime:
        if (inDataLengthCode >= 7) {
            // The year is in byte 5 and 6
            _dateTime.year = inData[5] + (inData[6] << 8);
            // The month is in bits 4-7 of byte 4
            _dateTime.month = inData[4] >> 4;
            // The day is in byte 3
            _dateTime.day = inData[3];
            // The hour is in byte 0
            _dateTime.hour = inData[0];
            // The minute is in byte 1
            _dateTime.minute = inData[1];
            // The second is in byte 2
            _dateTime.second = inData[2];
            _dateTime.additionalData.dataLengthCode = inDataLengthCode;
            if (inDataLengthCode == 8) {
                _dateTime.additionalData.uncodedDataByte7 = inData[7];
            }
            return true;
        }
        break;
    case Identifier::passengerSideFrontSeatSeatbeltAndSeatOccupancyStatus:
        if (inDataLengthCode >= 2) {
            // The seatbelt status is in bit 0 of byte 1
            _passengerSideFrontSeatSeatbeltAndSeatOccupancyStatus.seatbeltFastened = (inData[1] & 0x01) == 0x01;
            // When bits 2, 5 and 6 are 1 the seat is occupied
            _passengerSideFrontSeatSeatbeltAndSeatOccupancyStatus.occupied = (inData[1] & 0x64) == 0x64;
            return true;
        }
        break;
    case Identifier::doorOpenStatuses:
        if (inDataLengthCode >= 3) {
            // The front driver side door open status is in bit 0 of byte 1
            _doorOpenStatuses.frontDriverSideDoorIsOpen = (inData[1] & 0x01) != 0;
            // The front passenger side door open status is in bit 2 of byte 1
            _doorOpenStatuses.frontPassengerSideDoorIsOpen = (inData[1] & 0x04) != 0;
            // The rear driver side door open status is in bit 4 of byte 1
            _doorOpenStatuses.rearDriverSideDoorIsOpen = (inData[1] & 0x10) != 0;
            // The rear passenger side door open status is in bit 6 of byte 1
            _doorOpenStatuses.rearPassengerSideDoorIsOpen = (inData[1] & 0x40) != 0;
            // The boot open status is in bit 0 of byte 2
            _doorOpenStatuses.bootIsOpen = (inData[2] & 0x01) != 0;
            // The bonnet open status is in bit 2 of byte 2
            _doorOpenStatuses.bonnetIsOpen = (inData[2] & 0x04) != 0;
            return true;
        }
        break;
    case Identifier::handbrakeStatus:
        if (inDataLengthCode >= 1) {
            // Bits 0 and 1 of byte 0 are 2 when the handbrake is active
            _handbrakeStatus.handbrakeIsActive = (inData[0] & 0x03) == 0x02;
            return true;
        }
        break;
    default:
        break;
    }

    return false;
}

void CanCoder::Encode(uint32_t& outIdentifier, uint8_t& outDataLengthCode, uint8_t* outData) {
    outIdentifier = (uint32_t)_identifier;
    switch (_identifier) {
    case Identifier::iDriveControler:
        // The stick direction is encoded in byte 0
        // Each direction has a value rather than using seperate bits
        if (_iDriveController.stickUp) {
            outData[0] = 0x00;
        }
        else if (_iDriveController.stickRight) {
            outData[0] = 0x02;
        }
        else if (_iDriveController.stickDown) {
            outData[0] = 0x04;
        }
        else if (_iDriveController.stickLeft) {
            outData[0] = 0x06;
        }
        else {
            outData[0] = 0x0f;
        }
        // The buttons are encoded in bits 0, 2 and 4 of byte 1
        // Bits 6 and 7 are always 1
        outData[1] =
            (_iDriveController.stickPush ? 0x01 : 0) |
            (_iDriveController.homeButton ? 0x04 : 0) |
            (_iDriveController.menuButton ? 0x10 : 0) |
            0xc0;
        // The dial value is encode in bytes 2 and 3
        outData[2] = _iDriveController.dialValue & 0xff;
        outData[3] = (_iDriveController.dialValue >> 8) & 0xff;
        outDataLengthCode = _iDriveController.additionalData.dataLengthCode;
        if (outDataLengthCode < 4) {
            outDataLengthCode = 4;
        }
        for (int index = 4; index < _iDriveController.additionalData.dataLengthCode; index++) {
            outData[index] = _iDriveController.additionalData.uncodedData[index - 4];
        }
        break;
    case Identifier::gearShifterPosition:
        // It seems crazy to encode this. The only reason is to be able to trigger another device
        //
        // The automatic gear shifter position is encoded in bits 0-3 of byte 0
        // Bits 0-3 respectively represent P-R-N-D
        // Bits 4-7 are an inverted version of bits 0-3
        switch (_gearShifterPosition.position) {
        case 'P':
            outData[0] = 0xe1;
            break;
        case 'R':
            outData[0] = 0xd2;
            break;
        case 'N':
            outData[0] = 0xb4;
            break;
        case 'D':
            outData[0] = 0x78;
            break;
        }
        outDataLengthCode = _gearShifterPosition.additionalData.dataLengthCode;
        if (outDataLengthCode < 1) {
            outDataLengthCode = 1;
        }
        if (_gearShifterPosition.additionalData.dataLengthCode >= 4) {
            // Byte 3 is a counter incrementing with 0x10 wrapping around above 0xf0
            // Set this counter to the next value
            // Alter the uncoded data so the counter keeps incrementing for consecutive encodes
            // In the uncoded data it is at position 2
            _gearShifterPosition.additionalData.uncodedData[2] += 0x10;
            _gearShifterPosition.additionalData.uncodedData[2] %= 0xf0;
        }
        for (int index = 1; index < _gearShifterPosition.additionalData.dataLengthCode; index++) {
            outData[index] = _gearShifterPosition.additionalData.uncodedData[index - 1];
        }
        break;
    case Identifier::windowRoofAndMirrorControl:
        if (_windowRoofAndMirrorControl.closeWindowsAndRoof) {
            // Closing action for the windows and roof is encode with 0x1b in bytes 0 and 2
            outData[0] = 0x1b;
            outData[2] = 0x1b;
        }
        else {
            outData[0] = 0;
            outData[2] = 0;
        }
        if (_windowRoofAndMirrorControl.foldMirrors) {
            // Folding action for the mirrors is encode with 0x1b in byte 1
            outData[1] = 0x1b;
        }
        else {
            outData[1] = 0;
        }
        // If any action is to be performed byte 3 should be 0x52
        // For no action (stopping current action) it should be 0x50
        if (_windowRoofAndMirrorControl.closeWindowsAndRoof || _windowRoofAndMirrorControl.foldMirrors) {
            outData[3] = 0x52;
        }
        else {
            outData[3] = 0x50;
        }
        outDataLengthCode = _windowRoofAndMirrorControl.additionalData.dataLengthCode;
        if (outDataLengthCode < 4) {
            outDataLengthCode = 4;
        }
        for (int index = 4; index < _windowRoofAndMirrorControl.additionalData.dataLengthCode; index++) {
            outData[index] = _windowRoofAndMirrorControl.additionalData.uncodedData[index - 4];
        }
        break;
    case Identifier::doorLockControl:
        // As we can only encode a locking action for this identifier, the lockDoors bool is not checked
        // Door locking action is encoded in bytes 0-3
        outData[0] = 0x33;
        outData[1] = 0x33;
        outData[2] = 0x38;
        outData[3] = 0x00;
        outDataLengthCode = _doorLockControl.additionalData.dataLengthCode;
        if (outDataLengthCode < 4) {
            outDataLengthCode = 4;
        }
        for (int index = 4; index < _doorLockControl.additionalData.dataLengthCode; index++) {
            outData[index] = _doorLockControl.additionalData.uncodedData[index - 4];
        }
        break;
    case Identifier::setDateTime:
        // The year is in byte 5 and 6
        outData[5] = _dateTime.year & 0xff;
        outData[6] = (_dateTime.year >> 8) & 0xff;
        // The month is in bits 4-7 of byte 4
        // Bits 0-3 are set to 1
        outData[4] = (_dateTime.month << 4) | 0x0f;
        // The day is in byte 3
        outData[3] = _dateTime.day;
        // The hour is in byte 0
        outData[0] = _dateTime.hour;
        // The minute is in byte 1
        outData[1] = _dateTime.minute;
        // The second is in byte 2
        outData[2] = _dateTime.second;
        outDataLengthCode = _dateTime.additionalData.dataLengthCode;
        if (outDataLengthCode < 7) {
            outDataLengthCode = 7;
        }
        if (_dateTime.additionalData.dataLengthCode == 8) {
            outData[7] = _dateTime.additionalData.uncodedDataByte7;
        }
        break;
    default:
        break;
    }
}

std::string CanCoder::RawMessageToString(uint32_t identifier, uint8_t dataLengthCode, uint8_t* data) {
    std::stringstream rawMessageString;
    rawMessageString << std::setfill('0') << std::setw(3) << std::uppercase << std::hex << identifier << ":";
    for (int index = 0; index < dataLengthCode; index++) {
        rawMessageString << " " << std::setfill('0') << std::setw(2) << std::uppercase << std::hex << (uint32_t)data[index];
    }
    return rawMessageString.str();
}

std::string CanCoder::ToString() {
    std::stringstream messageString;
    switch (_identifier) {
    case Identifier::frontPassengerSideDoorStatus:
        messageString << "ID:FrontPassengerSideDoorStatus" << " open:" << _frontPassengerSideDoorStatus.open << " locked:" << _frontPassengerSideDoorStatus.locked;
        break;
    case Identifier::rearPassengerSideDoorStatus:
        messageString << "ID:RearPassengerSideDoorStatus" << " open:" << _rearPassengerSideDoorStatus.open << " locked:" << _rearPassengerSideDoorStatus.locked;
        break;
    case Identifier::frontDriverSideDoorStatus:
        messageString << "ID:FrontDriverSideDoorStatus" << " open:" << _frontDriverSideDoorStatus.open << " locked:" << _frontDriverSideDoorStatus.locked;
        break;
    case Identifier::rearDriverSideDoorStatus:
        messageString << "ID:RearDriverSideDoorStatus" << " open:" << _rearDriverSideDoorStatus.open << " locked:" << _rearDriverSideDoorStatus.locked;
        break;
    case Identifier::mirrorFoldStatus:
        messageString << "ID:MirrorFoldStatus" << " folded:" << _mirrorFoldStatus.folded;
        break;
    case Identifier::ignitionAndKeyLocation:
        messageString << "ID:IgnitionAndKeyLocation" << " keyIsOutside:" << _ignitionAndKeyLocation.keyIsOutside;
        break;
    case Identifier::vehicleSpeed:
        messageString << "ID:VehicleSpeed" << " speed:" << _vehicleSpeed.speed;
        break;
    case Identifier::iDriveControler:
        messageString <<
            "ID:IDriveControler" <<
            " stickUp:" << _iDriveController.stickUp << " stickRight:" << _iDriveController.stickRight <<
            " stickDown:" << _iDriveController.stickDown << " stickLeft:" << _iDriveController.stickLeft <<
            " stickPush:" << _iDriveController.stickPush << " homeButton:" << _iDriveController.homeButton <<
            " menuButton:" << _iDriveController.menuButton << " dialValue:" << _iDriveController.dialValue;
        break;
    case Identifier::gearShifterPosition:
        messageString << "ID:GearShifterPosition" << " position:" << _gearShifterPosition.position;
        break;
    case Identifier::remoteControlAndDoorHandleInput:
        messageString <<
            "ID:RemoteControlAndDoorHandleInput" <<
            " remoteControlUnlockButton:" << _remoteControlAndDoorHandleInput.remoteControlUnlockButton <<
            " remoteControlLockButton:" << _remoteControlAndDoorHandleInput.remoteControlLockButton <<
            " doorHandleUnlockButton:" << _remoteControlAndDoorHandleInput.doorHandleUnlockButton <<
            " doorHandleLockButton:" << _remoteControlAndDoorHandleInput.doorHandleLockButton;
        break;
    case Identifier::windowRoofAndMirrorControl:
        messageString << "ID:WindowRoofAndMirrorControl" << " closeWindowsAndRoof:" << _windowRoofAndMirrorControl.closeWindowsAndRoof << "foldMirrors:" << _windowRoofAndMirrorControl.foldMirrors;
        break;
    case Identifier::doorLockControl:
        messageString << "ID:DoorLockControl" << " lockDoors:" << _doorLockControl.lockDoors;
        break;
    case Identifier::dateTime:
    case Identifier::setDateTime:
        messageString << (_identifier == Identifier::dateTime ? "ID:DateTime" : "ID:SetDateTime") <<
            " " << _dateTime.year << "-" << std::setfill('0') << std::setw(2) << _dateTime.month << "-" << std::setfill('0') << std::setw(2) << _dateTime.day <<
            " " << std::setfill('0') << std::setw(2) << _dateTime.hour << ":" << std::setfill('0') << std::setw(2) << _dateTime.minute << ":" << std::setfill('0') << std::setw(2) << _dateTime.second;
        break;
    case Identifier::passengerSideFrontSeatSeatbeltAndSeatOccupancyStatus:
        messageString <<
            "ID:PassengerSideFrontSeatSeatbeltAndSeatOccupancyStatus" <<
            " seatbeltFastened:" << _passengerSideFrontSeatSeatbeltAndSeatOccupancyStatus.seatbeltFastened <<
            " occupied:" << _passengerSideFrontSeatSeatbeltAndSeatOccupancyStatus.occupied;
        break;
    case Identifier::doorOpenStatuses:
        messageString <<
            "ID:doorOpenStatuses" <<
            " frontDriverSideDoorIsOpen:" << _doorOpenStatuses.frontDriverSideDoorIsOpen << " frontPassengerSideDoorIsOpen:" << _doorOpenStatuses.frontPassengerSideDoorIsOpen <<
            " rearDriverSideDoorIsOpen:" << _doorOpenStatuses.rearDriverSideDoorIsOpen << " rearPassengerSideDoorIsOpen:" << _doorOpenStatuses.rearPassengerSideDoorIsOpen <<
            " bootIsOpen:" << _doorOpenStatuses.bootIsOpen << " bonnetIsOpen:" << _doorOpenStatuses.bonnetIsOpen;
        break;
    case Identifier::handbrakeStatus:
        messageString << "ID:handbrakeStatus" << " handbrakeIsActive:" << _handbrakeStatus.handbrakeIsActive;
        break;
   default:
        break;
    }
    return messageString.str();
}