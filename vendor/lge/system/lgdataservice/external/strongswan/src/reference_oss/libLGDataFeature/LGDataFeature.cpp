#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cutils/properties.h>
#include "LGDataFeature.h"

#define LOG_TAG "LGDataFeature"
#include <log/log.h>

extern "C" {

static bool _is_intialized = false;

///////////////////////////////////////////////////////////////////////////////////
// Const Operator

// Build or NT Code
#define BUILD_OPERATOR_PROP_NAME         "ro.vendor.lge.build.target_operator"
#define BUILD_COUNTRY_PROP_NAME          "ro.vendor.lge.build.target_country"
#define BUILD_REGION_PROP_NAME           "ro.vendor.lge.build.target_region"

static char gBuildOperator[LGDATA_VALUE_MAX] = {0};
static char gBuildCountry [LGDATA_VALUE_MAX] = {0};
static char gBuildRegion  [LGDATA_VALUE_MAX] = {0};
static bool gUseSimProperties             = false;

// Test for sim operator
#define SIM_OPERATOR_TEST_PROP_NAME      "persist.vendor.lge.data.sim.op.test"
static bool gUseTestSimProperties         = false;

// Smart CA
#define SMART_CA_OPERATOR_PROP_NAME      "persist.vendor.lge.cota.operator"
#define SMART_CA_COUNTRY_PROP_NAME       "persist.vendor.lge.cota.country"

///////////////////////////////////////////////////////////////////////////////////
// Sim Operator
#define LAOP_PROP_NAME                   "ro.vendor.lge.laop"
#define LAOP_USE_SIM_OPERATOR_PROP_NAME  "ro.vendor.lge.sim.operator.use"

static bool gIsLaopBuild                 = false;
static bool gUseLaopSimOperator          = false;

// LAOP
#define LAOP_SIM_OPERATOR_PROP_NAME      "persist.vendor.lge.sim.operator"
#define LAOP_SIM2_OPERATOR_PROP_NAME     "persist.vendor.lge.sim2.operator"

// LGDATA
#define DATA_SIM_OPERATOR_PROP_NAME      "persist.vendor.lge.data.sim_operator"
#define DATA_SIM2_OPERATOR_PROP_NAME     "persist.vendor.lge.data.sim2_operator"

#define LAOP_SIM_COUNTRY_PROP_NAME       "persist.vendor.lge.data.sim_country"
#define LAOP_SIM2_COUNTRY_PROP_NAME      "persist.vendor.lge.data.sim2_country"
#define LAOP_MSIM_COUNTRY_PROP_FMT       "persist.vendor.lge.sim%d.country"

#define LAOP_DEFAULT_OPERATOR_PROP_NAME  "ro.vendor.lge.laop.default.operator"

static char gLaopDefaultOperator [LGDATA_VALUE_MAX] = {0};

// Ril Card Operator
#define CARD_OPERATOR_PROP_NAME          "product.lge.ril.card_operator" //NOT_USED

// MSIM Configuration
#define MULTI_SIM_CONFIG_PROP_NAME       "persist.radio.multisim.config"
static int gPhoneCount                   = 1;

// SIM MCC/MNC Information
#define ICC_OPERATOR_NUMERIC_PROP_NAME   "gsm.sim.operator.numeric"

// FAKE SIM MCC/MNC information
#define FAKE_ICC_OPERATOR_NUMERIC_PROP_NAME "persist.vendor.lge.data.fake_operator_numeric"


int __get__phone__count() {

    char propVal [LGDATA_VALUE_MAX];
    memset(propVal, 0x0, LGDATA_VALUE_MAX);
    property_get(MULTI_SIM_CONFIG_PROP_NAME, propVal, "ss");
    if (strncasecmp(propVal, "dsds", 4) == 0 || strncasecmp(propVal, "dsda", 4) == 0) {
        return 2;
    } else if (strncasecmp(propVal, "tsts", 4) == 0) {
        return 3;
    }
    return 1;
}

bool __is__valid__slot__id(int subId) {

    if (subId < 0 || subId >= gPhoneCount) {
        return false;
    }
    return true;
}

bool __init__build__properties() {

    int len = 0;
    char propVal [LGDATA_VALUE_MAX];
    bool laopSimOperatorUseInitialized = false;

    if (_is_intialized) {
        return _is_intialized;
    }

    // init global valuables
    gIsLaopBuild = false;
    gUseLaopSimOperator = false;
    gUseSimProperties = false;
    gUseTestSimProperties = false;

    property_get(BUILD_OPERATOR_PROP_NAME, gBuildOperator, "Unknown");

    property_get(BUILD_COUNTRY_PROP_NAME, gBuildCountry, "Unknown");

    property_get(BUILD_REGION_PROP_NAME, gBuildRegion, "Unknown");

    // INTERNAL TEST PURPOSE [START]
    memset(propVal, 0x0, LGDATA_VALUE_MAX);
    len = property_get(SIM_OPERATOR_TEST_PROP_NAME, propVal, "false");
    if ((len > 0) && (strncasecmp(propVal, "true", 4) == 0 || strncasecmp(propVal, "1", 1) == 0)) {
        gUseTestSimProperties = true;
    }

    gUseSimProperties = (!strncasecmp(gBuildOperator, "OPEN", 4) || // OPEN GLOBAL, CA,US
            !strncasecmp(gBuildOperator, "CNO", 3) || // CNO
            !strncasecmp(gBuildOperator, "NAO", 3) || // NAO, AMZ
            !strncasecmp(gBuildOperator, "TRF", 3) || // TRF One Binary
            gUseTestSimProperties);

    memset(propVal, 0x0, LGDATA_VALUE_MAX);
    len = property_get(LAOP_PROP_NAME, propVal, "false");
    if ((len > 0) && (strncasecmp(propVal, "true", 4) == 0 || strncasecmp(propVal, "1", 1) == 0)) {
        gIsLaopBuild = true;
    }

    memset(propVal, 0x0, LGDATA_VALUE_MAX);
    len = property_get(LAOP_USE_SIM_OPERATOR_PROP_NAME, propVal, "");

    laopSimOperatorUseInitialized = (!gUseSimProperties || len > 0);

    gUseLaopSimOperator = false;
    if ((len > 0) && (strncasecmp(propVal, "true", 4) == 0 || strncasecmp(propVal, "1", 1) == 0)) {
        gUseLaopSimOperator = true;
    } else if (laopSimOperatorUseInitialized == false) {
        if (gIsLaopBuild) {
            gUseLaopSimOperator = gUseSimProperties;
        }
    }

    len = property_get(LAOP_DEFAULT_OPERATOR_PROP_NAME, gLaopDefaultOperator, "OPEN");

    gPhoneCount = __get__phone__count();

    if (gIsLaopBuild == false
            || laopSimOperatorUseInitialized) {
        _is_intialized = true;
    }

    ALOGI("__init__build__properties: gBuildOperator=%s, gBuildCountry=%s, gBuildRegion=%s, phoneCount=%d, _is_intialized=%d"
            , gBuildOperator, gBuildCountry, gBuildRegion, gPhoneCount, _is_intialized);
    ALOGI("__init__build__properties: gIsLaopBuild=%d, gUseLaopSimOperator=%d, gUseSimProperties=%d, gUseTestSimProperties=%d"
            , gIsLaopBuild, gUseLaopSimOperator, gUseSimProperties, gUseTestSimProperties);

    return _is_intialized;
}

bool __is__smart__ca__provisioned() {

    int len = 0;
    char smartCaOperator[LGDATA_VALUE_MAX];
    memset(smartCaOperator, 0x0, LGDATA_VALUE_MAX);
    len = property_get(SMART_CA_OPERATOR_PROP_NAME, smartCaOperator, "");

    return (len > 0);
}

int __get__sim__operator(char* _operator, int slotId) {

    int len = 0;
    __init__build__properties();

    if (_operator == NULL) {
        return 0;
    }

    if (__is__valid__slot__id(slotId) == false) {
        ALOGW("__get__sim__operator: parameter slotId is not valid, slotId=%d phoneCount=%d", slotId, gPhoneCount);
        _operator[0] = '\0';
        return 0;
    }

    if (gUseLaopSimOperator) {
        if (slotId == 0) {
            len = property_get(LAOP_SIM_OPERATOR_PROP_NAME, _operator, "");
        } else if (slotId ==1) {
            len = property_get(LAOP_SIM2_OPERATOR_PROP_NAME, _operator, "");
        } else {
            _operator[0] = '\0';
            return 0;
        }

        if (len == 0) {
            len = strlen(gLaopDefaultOperator);
            strncpy(_operator, gLaopDefaultOperator, len);
            _operator[len] = '\0';
        }
    } else if (gIsLaopBuild) {
        len = strlen(gLaopDefaultOperator);
        strncpy(_operator, gLaopDefaultOperator, len);
        _operator[len] = '\0';

        // TODO:  DOES NOT USED, NEED TO IMPROVE THIS LINES
        // Actually, in case of TMUS and MPCS operator, it does not reach here,
        // because getOperator() returns getConstOperator() for both operators
        // But, TMUS and MPCS are also included in LAOP build, hence these two lines are added currently.
        if (!strcmp(gLaopDefaultOperator, "TMUS")) { // TMUS, MPCS case
            len = strlen(gBuildCountry);
            strncpy(_operator, gBuildCountry, len);
            _operator[len] = '\0';
        }
    } else {
        if (slotId == 0) {
            len = property_get(DATA_SIM_OPERATOR_PROP_NAME, _operator, "");
        } else if (slotId == 1) {
            len = property_get(DATA_SIM2_OPERATOR_PROP_NAME, _operator, "");
        } else {
            _operator[0] = '\0';
            return 0;
        }

        if (len == 0) {
            len = strlen(gLaopDefaultOperator);
            strncpy(_operator, gLaopDefaultOperator, len);
            _operator[len] = '\0';
        }
    }

    // TODO: Need to check following cases
    // 1. What if VZW version has VZW SIM card, return VZW? How to decide it in NONE-LAOP version?
    // 2. What if VZW version has TMUS SIM card, return TMUS or RAW?

    return len;
}

int __get_sim_country(char* country, int slotId) {

    int len = 0;
    __init__build__properties();

    if (country == NULL) {
        return 0;
    }

    if (__is__valid__slot__id(slotId) == false) {
        ALOGW("__get_sim_country: parameter slotId is not valid, slotId=%d phoneCount=%d", slotId, gPhoneCount);
        country[0] = '\0';
        return 0;
    }

    if (gUseLaopSimOperator) {
        if (slotId == 0) {
            len = property_get(LAOP_SIM_COUNTRY_PROP_NAME, country, "");
        } else if (slotId ==1) {
            len = property_get(LAOP_SIM2_COUNTRY_PROP_NAME, country, "");
        } else {
            country[0] = '\0';
            return 0;
        }

        if (len == 0) {
            len = strlen(gBuildCountry);
            strncpy(country, gBuildCountry, len);
            country[len] = '\0';
        }
    } else {
        len = strlen(gBuildCountry);
        strncpy(country, gBuildCountry, len);
        country[len] = '\0';
    }

    // TODO: Need to check following cases
    // 1. What if VZW version has VZW SIM card, return US? How to decide it in NONE-LAOP version?
    // 2. What if VZW version has TMUS SIM card, return US or RAW?

    return len;
}


int get_const_operator(char* _operator) {

    int len = 0;
    __init__build__properties();

    if (_operator == NULL) {
        return 0;
    }

    len = strlen(gBuildOperator);
    strncpy(_operator, gBuildOperator, len);
    _operator[len] = '\0';

    if (__is__smart__ca__provisioned()) {
        char smartCaOperator[LGDATA_VALUE_MAX];
        memset(smartCaOperator, 0x0, LGDATA_VALUE_MAX);
        len = property_get(SMART_CA_OPERATOR_PROP_NAME, smartCaOperator, "");

        if (len > 0) {
            strncpy(_operator, smartCaOperator, len);
            _operator[len] = '\0';
        }
    }

    return strlen(_operator);
}

int get_const_country(char* country) {

    int len = 0;
    __init__build__properties();

    if (country == NULL) {
        return 0;
    }

    len = strlen(gBuildCountry);
    strncpy(country, gBuildCountry, len);
    country[len] = '\0';

    if (__is__smart__ca__provisioned()) {
        char smartCaCountry[LGDATA_VALUE_MAX];
        memset(smartCaCountry, 0x0, LGDATA_VALUE_MAX);
        len = property_get(SMART_CA_COUNTRY_PROP_NAME, smartCaCountry, "");

        if (len > 0) {
            strncpy(country, smartCaCountry, len);
            country[len] = '\0';
        } else {
            ALOGW("get_const_country: SmartCA country is empty even if SmartCA is completed");
        }
    }

    return strlen(country);
}

int get_operator(char* _operator, int slotId) {

    int ret = 0;
    __init__build__properties();

    if (_operator == NULL) {
        return 0;
    }

    if (gUseSimProperties && !__is__smart__ca__provisioned()) {
        ret = __get__sim__operator(_operator, slotId);
        if (ret > 0) {
            return ret;
        }
    }

    return get_const_operator(_operator);
}

int get_country(char* country, int slotId) {

    int ret = 0;
    __init__build__properties();

    if (country == NULL) {
        return 0;
    }

    if ((gUseSimProperties && !__is__smart__ca__provisioned()) ||
            (!LGDataFeature_is_const_operator("OPEN") && LGDataFeature_is_const_country("COM"))) {
        ret = __get_sim_country(country, slotId);
        if (ret > 0) {
            return ret;
        }

    }

    return get_const_country(country);
}

int get_const_region(char* region) {

    int len = 0;
    __init__build__properties();

    if (region == NULL) {
        return 0;
    }

    len = strlen(gBuildRegion);
    strncpy(region, gBuildRegion, len);
    region[len] = '\0';

    return len;
}

int get_sim_mcc(char* mcc, int slotId) {

    int i = 0;
    int len = 0;

    if (mcc == NULL) {
        return 0;
    }

    char numeric[LGDATA_VALUE_MAX];
    memset(numeric, 0x0, LGDATA_VALUE_MAX);
    len = property_get(ICC_OPERATOR_NUMERIC_PROP_NAME, numeric, "");

    if (len < 5) {
        mcc[0] = '\0';
        return 0;
    }

    char* start = &numeric[0];
    char* end = NULL;

    // Find Slot Id
    end = strchr(numeric, ',');
    for (i = 0; i < slotId; ++i) {
        if (end == NULL) {
            mcc[0] = '\0';
            return 0;
        }
        start = end + 1;
        end = strchr(start, ',');
    }

    if (end == NULL) {
        len = strlen(start);
    }
    else {
        len = end - start;
    }

    if (len < 5) { // It should be 5 bytes long at least
        mcc[0] = '\0';
        return 0;
    }

    strncpy(mcc, start, 3);
    mcc[3] = '\0';
    return 3;
}

int get_sim_mnc(char* mnc, int slotId) {

    int i = 0;
    int len = 0;

    if (mnc == NULL) {
        return 0;
    }

    char numeric[LGDATA_VALUE_MAX];
    memset(numeric, 0x0, LGDATA_VALUE_MAX);
    len = property_get(ICC_OPERATOR_NUMERIC_PROP_NAME, numeric, "");

    if (len < 5) {
        mnc[0] = '\0';
        return 0;
    }

    char* start = &numeric[0];
    char* end = NULL;

    // Find Slot Id
    end = strchr(numeric, ',');
    for (i = 0; i < slotId; ++i) {
        if (end == NULL) {
            mnc[0] = '\0';
            return 0;
        }
        start = end + 1;
        end = strchr(start, ',');
    }

    if (end == NULL) {
        len = strlen(start);
    }
    else {
        len = end - start;
    }

    if (len < 5) { // It should be 5 bytes long at least
        mnc[0] = '\0';
        return 0;
    }

    strncpy(mnc, start+3, len-3);
    mnc[len-3] = '\0';
    return len-3;
}

bool LGDataFeature_is_operator(const char* _operator, int slotId) {
    char op[LGDATA_VALUE_MAX];
    memset(op, 0x0, LGDATA_VALUE_MAX);
    get_operator(op, slotId);
    return !strcasecmp(op, _operator);
}

bool LGDataFeature_is_country(const char* _country,  int slotId) {
    char country[LGDATA_VALUE_MAX];
    memset(country, 0x0, LGDATA_VALUE_MAX);
    get_country(country, slotId);
    return !strcasecmp(country, _country);
}

bool LGDataFeature_is_const_operator(const char* _operator) {
    char constOperator[LGDATA_VALUE_MAX];
    memset(constOperator, 0x0, LGDATA_VALUE_MAX);
    get_const_operator(constOperator);
    return !strcasecmp(constOperator, _operator);
}

bool LGDataFeature_is_const_country(const char* _country) {
    char constCountry[LGDATA_VALUE_MAX];
    memset(constCountry, 0x0, LGDATA_VALUE_MAX);
    get_const_country(constCountry);
    return !strcasecmp(constCountry, _country);
}

bool LGDataFeature_is_const_region(const char* _region) {
    char constRegion[LGDATA_VALUE_MAX];
    memset(constRegion, 0x0, LGDATA_VALUE_MAX);
    get_const_region(constRegion);
    return !strcasecmp(constRegion, _region);
}

int get_fake_mcc_mnc(char * _fake_numeric) {
    return property_get(FAKE_ICC_OPERATOR_NUMERIC_PROP_NAME, _fake_numeric, "");
}

int get_kr_sim_operator(char* _operator) {

    __init__build__properties();

    if (_operator == NULL) {
        return 0;
    }

    if (LGDataFeature_is_const_country("KR")) {
        int len = 0;
        char numeric[LGDATA_VALUE_MAX];
        memset(numeric, 0x0, LGDATA_VALUE_MAX);
        len = property_get(ICC_OPERATOR_NUMERIC_PROP_NAME, numeric, "");

        if (len >= 5) {
            if (!strncmp(numeric, "45005", 5) ||
                    !strncmp(numeric, "45011", 5)) {
                strncpy(_operator, "SKT", 3);
                _operator[3] = '\0';
                return strlen(_operator);
            }

            if (!strncmp(numeric, "45002", 5) ||
                    !strncmp(numeric, "45008", 5)) {
                strncpy(_operator, "KT", 2);
                _operator[2] = '\0';
                return strlen(_operator);
            }

            if (!strncmp(numeric, "45006", 5)) {
                strncpy(_operator, "LGU", 3);
                _operator[3] = '\0';
                return strlen(_operator);
            }

            if (!strncmp(numeric, "001", 3) ||
                    !strncmp(numeric, "00211", 5)) {
                strncpy(_operator, "TEST", 4);
                _operator[4] = '\0';
                return strlen(_operator);
            }
        }
    }

    strncpy(_operator, "NotSupported", 12);
    _operator[12] = '\0';

    return strlen(_operator);
}

int get_jp_sim_operator(char* _operator) {

    __init__build__properties();

    if (_operator == NULL) {
        return 0;
    }

    if (LGDataFeature_is_const_country("JP")) {
        int len = 0;
        int mcc = 0;
        int mnc = 0;
        char mccmnc[LGDATA_VALUE_MAX];
        char numeric[LGDATA_VALUE_MAX];
        memset(numeric, 0x0, LGDATA_VALUE_MAX);

        len = get_fake_mcc_mnc(numeric); // see if fake prop is set
        if (len >= 5) {
            memset(mccmnc, 0x0, LGDATA_VALUE_MAX);
            strncpy(mccmnc, numeric, 3);
            mccmnc[3] = '\0';
            mcc = atoi(mccmnc);

            memset(mccmnc, 0x0, LGDATA_VALUE_MAX);
            strncpy(mccmnc, numeric + 3, len-3);
            mccmnc[len-3] = '\0';
            mnc = atoi(mccmnc);
        } else {
            len = property_get(ICC_OPERATOR_NUMERIC_PROP_NAME, numeric, "");

            memset(mccmnc, 0x0, LGDATA_VALUE_MAX);
            get_sim_mcc(mccmnc, LGDATA_SLOT_PRIMARY);
            mcc = atoi(mccmnc);

            memset(mccmnc, 0x0, LGDATA_VALUE_MAX);
            get_sim_mnc(mccmnc, LGDATA_SLOT_PRIMARY);
            mnc = atoi(mccmnc);
        }

        if (len >= 5) {
            if (!strncmp(numeric, "44010", 5)) {
                strncpy(_operator, "DCM", 3);
                _operator[3] = '\0';
                return strlen(_operator);
            }

            if (mcc == 441 && mnc == 70) {
                strncpy(_operator, "KDDI", 4);
                _operator[4] = '\0';
                return strlen(_operator);
            }

            if (mcc == 440) {
                if (mnc == 7 || mnc == 8 || mnc == 88 || mnc == 89 ||
                        (mnc >= 50 && mnc <= 56) ||
                        (mnc >= 70 && mnc <= 79)) {
                    strncpy(_operator, "KDDI", 4);
                    _operator[4] = '\0';
                    return strlen(_operator);
                }
            }

            /* JCM Case? */
            /*
            if (!strncmp(numeric, "440yy", 5) ||
                    !strncmp(numeric, "440xx", 5)) {
                strncpy(_operator, "JCM", 3);
                _operator[3] = '\0';
                return strlen(_operator);
            }
            */

            if (!strncmp(numeric, "44000", 5) ||
                    !strncmp(numeric, "44020", 5) ||
                    !strncmp(numeric, "44021", 5) ||
                    !strncmp(numeric, "44101", 5)) {
                strncpy(_operator, "SB", 2);
                _operator[2] = '\0';
                return strlen(_operator);
            }

            if (!strncmp(numeric, "001", 3)) {
                strncpy(_operator, "TEST", 4);
                _operator[4] = '\0';
                return strlen(_operator);
            }
        }
    }

    strncpy(_operator, "NotSupported", 12);
    _operator[12] = '\0';

    return strlen(_operator);
}

bool LGDataFeature_is_kr_sim_operator(const char* _operator) {
    char simOperator[LGDATA_VALUE_MAX];
    memset(simOperator, 0x0, LGDATA_VALUE_MAX);
    get_kr_sim_operator(simOperator);
    return !strcasecmp(simOperator, _operator);
}

bool LGDataFeature_is_jp_sim_operator(const char* _operator) {
    char simOperator[LGDATA_VALUE_MAX];
    memset(simOperator, 0x0, LGDATA_VALUE_MAX);
    get_jp_sim_operator(simOperator);
    return !strcasecmp(simOperator, _operator);
}

bool LGDataFeature_is_const_global_operator() {
    bool bCountryKR = !strncasecmp(gBuildCountry, "KR", 2);
    bool bCountryJP = !strncasecmp(gBuildCountry, "JP", 2);
    bool bCountryUS = !strncasecmp(gBuildCountry, "US", 2);
    bool bCountryCA = !strncasecmp(gBuildCountry, "CA", 2);
    bool bOperatorDPAC = !strncasecmp(gBuildOperator, "DPAC", 4);

    bool bTreatAsGlobalCountry = !bCountryKR && !bCountryJP && !bCountryUS && !bCountryCA;

    return bTreatAsGlobalCountry || bOperatorDPAC;
}

bool __is_open_device() {
    return !strncasecmp(gBuildOperator, "OPEN", 4) && strncasecmp(gBuildCountry, "CA", 2);
}

} // extern "C"
