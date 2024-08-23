/*
 * SPDX-FileCopyrightText: Hans Lennaerts (cancoder@lennaerts.eu)
 *
 * SPDX-License-Identifier: MIT
 *
 *
 * CanCoder
 *
 * Decoding and encoding of CAN bus messages
 * 
 * Currently supported is a subset of the messages for the BMW Mini R60
 * There may be overlap with other BMW vehicles
 *
 * When decoding, only the relevant fields are decoded from the data. All non-decoded
 * data however is stored seperately for each identifier. For each identifier there is
 * a struct member holding it's data.
 * This way, encoding will always produce a complete message containing both decoded
 * and non-decoded data based on the latest decode for that identifier. If no decode
 * was performed yet the non-decoded data is filled to the best of knowledge.
 * For example:
 * You decode a message for identifier x, containing fields a and b and undecoded
 * bytes 4 and 5. Then you decode and encode some messages for other identifiers and
 * then you decide to encode a message for identifier x, but you only change field a.
 * What happens is that field b will be encoded as it was decoded previously and bytes
 * 4 and 5 that were not decoded will now still be included, resulting in a complete
 * message.
 *
 *
 * File history:
 * Version 1: initial
 *
 */

#pragma once

#include <stdint.h>
#include <string>

class CanCoder {
public:
    /// @brief Decode a CAN message
    /// @param inIdentifier CAN message identifier
    /// @param inDataLengthCode CAN message data length code
    /// @param inData CAN message data
    /// @return true on success, false on failure
    bool Decode(uint32_t inIdentifier, uint8_t inDataLengthCode, uint8_t* inData);
    /// @brief Encode a CAN message
    /// @param outIdentifier CAN message identifier
    /// @param outDataLengthCode CAN message data length code
    /// @param outData CAN message data, must be able to hold 8 bytes
    void Encode(uint32_t& outIdentifier, uint8_t& outDataLengthCode, uint8_t* outData);
    /// @brief Create a string for logging, showing the raw CAN message in hexadecimal format
    ///
    /// This function is static as it requires no state
    /// @param identifier CAN message identifier
    /// @param dataLengthCode CAN message data length code
    /// @param data CAN message data
    /// @return A string you can use for logging
    static std::string RawMessageToString(uint32_t identifier, uint8_t dataLengthCode, uint8_t* data);
    /// @brief Create a string for logging, showing the contents of the CAN message stored in this CanCoder instance
    /// @return A string you can use for logging
    std::string ToString();

    // CAN IDs
    enum class Identifier
    {
        frontPassengerSideDoorStatus = 0x0E2,
        rearPassengerSideDoorStatus = 0x0E6,
        frontDriverSideDoorStatus = 0x0EA,
        rearDriverSideDoorStatus = 0x0EE,
        mirrorFoldStatus = 0x0F6,
        ignitionAndKeyLocation = 0x130,
        vehicleSpeed = 0x1B4,
        iDriveControler = 0x1B8,
        gearShifterPosition = 0x1D2,
        remoteControlAndDoorHandleInput = 0x23A,
        windowRoofAndMirrorControl = 0x26E,
        doorLockControl = 0x2A0,
        dateTime = 0x2F8,
        passengerSideFrontSeatSeatbeltAndSeatOccupancyStatus = 0x2FA,
        doorOpenStatuses = 0x2FC,
        handbrakeStatus = 0x34F,
        setDateTime = 0x39E
    } _identifier = (Identifier)0;
    struct FrontPassengerSideDoorStatus {
        bool open;
        bool locked;
    } _frontPassengerSideDoorStatus = {};
    struct RearPassengerSideDoorStatus {
        bool open;
        bool locked;
    } _rearPassengerSideDoorStatus = {};
    struct FrontDriverSideDoorStatus {
        bool open;
        bool locked;
    } _frontDriverSideDoorStatus = {};
    struct RearDriverSideDoorStatus {
        bool open;
        bool locked;
    } _rearDriverSideDoorStatus = {};
    struct MirrorFoldStatus {
        bool folded;
    } _mirrorFoldStatus = {};
    struct IgnitionAndKeyLocation {
        bool keyIsOutside;
    } _ignitionAndKeyLocation = {};
    struct VehicleSpeed {
        int speed;
    } _vehicleSpeed = {};
    struct IDriveController {
        unsigned short dialValue;
        bool homeButton;
        bool menuButton;
        bool stickUp;
        bool stickRight;
        bool stickDown;
        bool stickLeft;
        bool stickPush;
        struct AdditionalData {
            uint8_t dataLengthCode = 6;
            uint8_t uncodedData[4] = { 0x14, 0x10 };
        } additionalData;
    } _iDriveController = {};
    struct GearShifterPosition {
        char position;
        struct AdditionalData {
            uint8_t dataLengthCode = 6;
            uint8_t uncodedData[7] = { 0x0f, 0xf0, 0x0c, 0xf0, 0xff };
        } additionalData;
    } _gearShifterPosition = {};
    struct RemoteControlAndDoorHandleInput {
        bool remoteControlUnlockButton;
        bool remoteControlLockButton;
        bool doorHandleUnlockButton;
        bool doorHandleLockButton;
    } _remoteControlAndDoorHandleInput = {};
    struct WindowRoofAndMirrorControl {
        bool closeWindowsAndRoof;
        bool foldMirrors;
        struct AdditionalData {
            uint8_t dataLengthCode = 8;
            uint8_t uncodedData[4] = { 0xff, 0xff, 0xff, 0xff };
        } additionalData;
    } _windowRoofAndMirrorControl = {};
    struct DoorLockControl {
        bool lockDoors;
        struct AdditionalData {
            uint8_t dataLengthCode = 8;
            uint8_t uncodedData[4] = {};
        } additionalData;
    } _doorLockControl = {};
    struct DateTime {
        int year;
        int month;
        int day;
        int hour;
        int minute;
        int second;
        struct AdditionalData {
            uint8_t dataLengthCode = 8;
            uint8_t uncodedDataByte7 = 0;
        } additionalData;
    } _dateTime = {};
    struct PassengerSideFrontSeatSeatbeltAndSeatOccupancyStatus {
        bool seatbeltFastened;
        bool occupied;
    } _passengerSideFrontSeatSeatbeltAndSeatOccupancyStatus = {};
    struct DoorOpenStatuses {
        bool frontDriverSideDoorIsOpen;
        bool frontPassengerSideDoorIsOpen;
        bool rearDriverSideDoorIsOpen;
        bool rearPassengerSideDoorIsOpen;
        bool bootIsOpen;
        bool bonnetIsOpen;
    } _doorOpenStatuses = {};
    struct HandbrakeStatus {
        bool handbrakeIsActive;
    } _handbrakeStatus = {};
};