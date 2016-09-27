#include "fm2spi.h"
#include "lmice_trace.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h> 

#include "fm3in1.h"

static char g_order_status;

#define update_localid(local_id) do {   \
    int id = local_id;  \
    int i;  \
    for(i=0;i<12;++i) { \
        g_order.UserOrderLocalID[11-i] = '0'+ (id % 10);    \
        id = id / 10;   \
    }   \
} while(0)

#define update_order_localid(local_id) do {   \
			int id = local_id;	\
			int i;	\
			for(i=0;i<12;++i) { \
				g_order_action.UserOrderLocalID[11-i] = '0'+ (id % 10); \
				id = id / 10;	\
			}	\
	} while(0)


#define update_order_action_localid(local_id) do {   \
		int id = local_id;	\
		int i;	\
		for(i=0;i<12;++i) { \
			g_order_action.UserOrderActionLocalID[11-i] = '0'+ (id % 10);	\
			id = id / 10;	\
		}	\
} while(0)


///< Utilities
forceinline void login(CFemas2TraderSpi* spi)
{
    CUstpFtdcReqUserLoginField req;
    memset(&req, 0, sizeof req);
    strncpy(req.BrokerID, spi->broker_id(), sizeof(req.BrokerID)-1);
    strncpy(req.UserID, spi->user_id(), sizeof(req.UserID)-1);
    strncpy(req.Password, spi->password(), sizeof(req.Password)-1);
    spi->trader()->ReqUserLogin(&req, spi->req_id());
}

forceinline void logout(CFemas2TraderSpi* spi) {
    CUstpFtdcReqUserLogoutField req;
    memset(&req, 0, sizeof req);
    strncpy(req.BrokerID, spi->broker_id(), sizeof(req.BrokerID)-1);
    strncpy(req.UserID, spi->user_id(), sizeof(req.UserID)-1);

    spi->trader()->ReqUserLogout(&req, spi->req_id());
}

forceinline void orderinsert(CFemas2TraderSpi* spi, CUstpFtdcInputOrderField* preq) {
    int local_id = spi->req_id();
    update_localid(local_id);

    spi->trader()->ReqOrderInsert(preq, local_id);
	
}

forceinline void orderaction(CFemas2TraderSpi* spi) 
{

	int local_id = g_spi->current_id();
    update_order_localid(local_id);
	local_id = g_spi->req_id();
	update_order_action_localid(local_id);
	strcpy( g_order_action.OrderSysID, g_cur_sysid );
	spi->trader()->ReqOrderAction( &g_order_action, local_id );

	int64_t systime = 0;
	get_system_time(&systime);

	//g_order_action_count++;

	printf("== send order action %d times ==\n UserOrderLocalID=%s\t UserOrderActionLocalID=%s\t OrderSysID=%s\n send action time: %ld \t interval: %ld\n\n", 
		g_order_action_count,
		g_order_action.UserOrderLocalID, 
		g_order_action.UserOrderActionLocalID, 
		g_order_action.OrderSysID,
		systime, systime - g_end_time);
	
}

forceinline void investoraccount(CFemas2TraderSpi* spi)
{
	CUstpFtdcQryInvestorAccountField qry;
	memset( &qry, 0, sizeof(CUstpFtdcQryInvestorAccountField) );
	strcpy(qry.BrokerID, spi->broker_id());
	strcpy(qry.UserID, spi->user_id());
	
	spi->trader()->ReqQryInvestorAccount( &qry, spi->req_id());
}

forceinline void investorposition(CFemas2TraderSpi* spi)
{
	CUstpFtdcQryInvestorPositionField qry;
	memset( &qry, 0, sizeof(CUstpFtdcQryInvestorPositionField) );
	strcpy( qry.BrokerID, spi->broker_id() );
	strcpy( qry.UserID, spi->user_id() );
	strcpy( qry.InvestorID, spi->investor_id() );
	int local_id = spi->req_id();
	printf("local_id:%d\n", local_id);
	
	spi->trader()->ReqQryInvestorPosition( &qry, local_id );
}

forceinline void hard_flatten(CFemas2TraderSpi* spi)
{
	if( g_cur_st->m_pos.m_buy_pos > 0 )
	{
		g_order.Volume = g_cur_st->m_pos.m_buy_pos;
		g_order.OffsetFlag = USTP_FTDC_OF_CloseToday;
		g_order.Direction = USTP_FTDC_D_Sell;
    	g_order.LimitPrice = g_cur_st->m_md.m_down_price;
		orderinsert( spi, &g_order );
	}

	if( g_cur_st->m_pos.m_sell_pos > 0 )
	{
		g_order.Volume = g_cur_st->m_pos.m_sell_pos;
		g_order.OffsetFlag = USTP_FTDC_OF_CloseToday;
		g_order.Direction = USTP_FTDC_D_Buy;
    	g_order.LimitPrice = g_cur_st->m_md.m_up_price;
		orderinsert( spi, &g_order );
	}
	
}

forceinline void soft_flatten(CFemas2TraderSpi* spi)
{

    if( g_cur_st->m_pos.m_buy_pos == g_cur_st->m_pos.m_sell_pos )
	{
		return;
	}

    if( g_cur_st->m_pos.m_buy_pos > g_cur_st->m_pos.m_sell_pos )
	{
        g_order.Volume = g_cur_st->m_pos.m_buy_pos - g_cur_st->m_pos.m_sell_pos;
		g_order.OffsetFlag = USTP_FTDC_OF_CloseToday;
		g_order.Direction = USTP_FTDC_D_Sell;
        g_order.LimitPrice = g_cur_st->m_md.m_down_price;
	}
	else
	{
        g_order.Volume = g_cur_st->m_pos.m_sell_pos - g_cur_st->m_pos.m_buy_pos;
		g_order.OffsetFlag = USTP_FTDC_OF_CloseToday;
		g_order.Direction = USTP_FTDC_D_Buy;
        g_order.LimitPrice = g_cur_st->m_md.m_up_price;
	}
	orderinsert( spi, &g_order );
	
}


CFemas2TraderSpi::CFemas2TraderSpi(CUstpFtdcTraderApi *pt, const char *name)
    :CLMSpi(name, 0)
{
    m_trader= pt;
    m_curid = 0;
    m_state = FMTRADER_UNKNOWN;
    m_user_id = NULL;
    m_password = NULL;
    m_broker_id = NULL;
    m_front_address = NULL;
    m_investor_id = NULL;
    m_model_name = NULL;

	
}

CFemas2TraderSpi::~CFemas2TraderSpi() {
    delete_spi();
}

int CFemas2TraderSpi::req_id()
{
    return ++m_curid;
}

int CFemas2TraderSpi::current_id() const
{
    return m_curid;
}

CUstpFtdcTraderApi* CFemas2TraderSpi::trader() const {
    return m_trader;
}

EFEMAS2TRADER CFemas2TraderSpi::status() const
{
    return m_state;
}

const char* CFemas2TraderSpi::user_id() const {
    return m_user_id;
}

const char* CFemas2TraderSpi::password() const {
    return m_password;
}

const char* CFemas2TraderSpi::broker_id() const {
    return m_broker_id;
}
const char* CFemas2TraderSpi::front_address() const {
    return m_front_address;
}

const char* CFemas2TraderSpi::investor_id() const {
    return m_investor_id;
}
const char* CFemas2TraderSpi::model_name() const {
    return m_model_name;
}

const char *CFemas2TraderSpi::exchange_id() const
{
    return m_exchange_id;
}
void CFemas2TraderSpi::user_id(const char* id) {
    m_user_id = id;
}

void CFemas2TraderSpi::password(const char* id) {
    m_password = id;
}

void CFemas2TraderSpi::broker_id(const char* id) {
    m_broker_id = id;
}

void CFemas2TraderSpi::front_address(const char* id) {
    m_front_address = id;
}

void CFemas2TraderSpi::investor_id(const char* id) {
    m_investor_id = id;
}

void CFemas2TraderSpi::model_name(const char* id) {
    m_model_name = id;
}

void CFemas2TraderSpi::exchange_id(const char *id)
{
    m_exchange_id = id;
}



#define MODELNAME(s, m, t) do{\
    memset(symbol, 0, sizeof(symbol));  \
    strncat(symbol, m, sizeof(symbol)-1);   \
    strncat(symbol, t, sizeof(symbol) -1);  \
} while(0)

    //[modelname]-req-flatten
int CFemas2TraderSpi::init_trader() {

}

void CFemas2TraderSpi::order_insert(const char *symbol, const void *addr, int size)
{
    (void)symbol;
    (void)addr;
    (void)size;


    CUstpFtdcInputOrderField* req = (CUstpFtdcInputOrderField*)addr;
    orderinsert(this, req);

}

void CFemas2TraderSpi::order_action(const char *symbol, const void *addr, int size)
{
    (void)symbol;
    (void)addr;
    (void)size;

	if( g_order_buy_price >0 || g_order_sell_price > 0)
	{
		orderaction(this);
	}
	
}


#define HARD_FLATTEN
void CFemas2TraderSpi::flatten_all(const char *symbol, const void *addr, int size)
{
    (void)symbol;
    (void)addr;
    (void)size;
    lmice_critical_print("flatten all\n");
	
#ifdef SOFT_FLATTEN
	soft_flatten(this);
#endif

#ifdef HARD_FLATTEN
	hard_flatten(this);
#endif

    m_state = FMTRADER_UNKNOWN;
}

void CFemas2TraderSpi::flatten_extra()
{
    lmice_critical_print("flatten extra\n");
	soft_flatten(this);
}


void CFemas2TraderSpi::trade_instrument(const char *symbol, const void *addr, int size)
{
    (void)symbol;
    (void)addr;
    (void)size;
    lmice_critical_print("====trade instrument====\n");
    lmice_info_print("get investor account state\n");
	investoraccount(this);
	//investorposition(this);
}

///< Callbacks

void CFemas2TraderSpi::OnFrontConnected()
{
    pthread_setname_np(pthread_self(), "Femas2Trader");
    m_state = FMTRADER_LOGIN;
    lmice_info_print("Femas2Trader[%x] connected, do login\n", pthread_self());
    login(this);
}

void CFemas2TraderSpi::OnFrontDisconnected(int nReason)
{
    m_state = FMTRADER_DISCONNECTED;
    lmice_error_print("Femas2Trader disconnected[%d]\n", nReason);
}

void CFemas2TraderSpi::OnRspUserLogin(CUstpFtdcRspUserLoginField *pRspUserLogin, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    (void)pRspUserLogin;
    (void)nRequestID;
    (void)bIsLast;
    if (pRspInfo!=NULL&&pRspInfo->ErrorID!=0)
    {

        lmice_error_print("Login failed(%s0 as error id[%d]\n",gbktoutf8( pRspInfo->ErrorMsg).c_str(),
                          pRspInfo->ErrorID );
        m_state = FMTRADER_CONNECTED;
        return;
    }
    lmice_info_print("Login successed\n");
	m_curid = atoi(pRspUserLogin->MaxOrderLocalID)+1;
    m_state = FMTRADER_LOGIN;

    CUstpFtdcQryInstrumentField req;
    memset(&req, 0, sizeof(req));
    strcpy(req.ExchangeID, exchange_id());
    strcpy(req.InstrumentID, trading_instrument);
    //strcpy(req.ProductID, "fmdemo");
    trader()->ReqQryInstrument(&req, req_id());
    lmice_info_print("do ReqQryInstrument\n");



/*
	CUstpFtdcInputOrderField req;
	memset(&req, 0, sizeof(CUstpFtdcInputOrderField));
	orderinsert(this, &req);
*/	
//	lmice_info_print("get investor account state\n");
//	investoraccount(this);
	
}

void CFemas2TraderSpi::OnRspUserLogout(CUstpFtdcRspUserLogoutField *pRspUserLogout, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    (void)pRspUserLogout;
    (void)nRequestID;
    (void)bIsLast;
    if (pRspInfo!=NULL&&pRspInfo->ErrorID!=0)
    {

        lmice_error_print("Logout failed(%s0 as error id[%d]\n",gbktoutf8( pRspInfo->ErrorMsg).c_str(),
                          pRspInfo->ErrorID );
        m_state = FMTRADER_CONNECTED;
        return;
    }
    lmice_info_print("logout\n");
}

void CFemas2TraderSpi::OnRspQryInvestorAccount(CUstpFtdcRspInvestorAccountField *pRspInvestorAccount, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (pRspInfo!=NULL&&pRspInfo->ErrorID!=0)
    {

        lmice_error_print("Logout failed(%s0 as error id[%d]\n",gbktoutf8( pRspInfo->ErrorMsg).c_str(),
                          pRspInfo->ErrorID );
        m_state = FMTRADER_CONNECTED;
        return;
    }
	lmice_info_print("OnRspQryInvestorAccount\n");

	printf("Request ID:%d\n", nRequestID);
	printf("BrokerID:%s\n", pRspInvestorAccount->BrokerID);
	printf("InvestorID:%s\n", pRspInvestorAccount->InvestorID);
	printf("AccountID:%s\n", pRspInvestorAccount->AccountID);
	printf("PreBalance:%lf\n", pRspInvestorAccount->PreBalance);
	printf("Deposit:%lf\n", pRspInvestorAccount->Deposit);
	printf("Withdraw:%lf\n", pRspInvestorAccount->Withdraw);
	printf("FrozenMargin:%lf\n", pRspInvestorAccount->FrozenMargin);
	printf("FrozenFee:%lf\n", pRspInvestorAccount->FrozenFee);
	printf("FrozenPremium:%lf\n", pRspInvestorAccount->FrozenPremium);
	printf("Fee:%lf\n", pRspInvestorAccount->Fee);
	printf("CloseProfit:%lf\n", pRspInvestorAccount->CloseProfit);
	printf("PositionProfit:%lf\n", pRspInvestorAccount->PositionProfit);
	printf("Available:%lf\n", pRspInvestorAccount->Available);
	printf("LongFrozenMargin:%lf\n", pRspInvestorAccount->LongFrozenMargin);
	printf("ShortFrozenMargin:%lf\n", pRspInvestorAccount->ShortFrozenMargin);
	printf("LongMargin:%lf\n", pRspInvestorAccount->LongMargin);
	printf("ShortMargin:%lf\n", pRspInvestorAccount->ShortMargin);
	printf("ReleaseMargin:%lf\n", pRspInvestorAccount->ReleaseMargin);
	printf("DynamicRights:%lf\n", pRspInvestorAccount->DynamicRights);
	printf("TodayInOut:%lf\n", pRspInvestorAccount->TodayInOut);
	printf("Margin:%lf\n", pRspInvestorAccount->Margin);
	printf("Premium:%lf\n", pRspInvestorAccount->Premium);
	printf("Risk:%lf\n", pRspInvestorAccount->Risk);
	printf("BeLast:%d\n", bIsLast);

    g_cur_status.m_acc.m_count_id = atoi(pRspInvestorAccount->AccountID);
    g_cur_status.m_acc.m_left_cash = 0;
    g_cur_status.m_acc.m_valid_val = pRspInvestorAccount->Available;
    g_cur_status.m_acc.m_total_val = pRspInvestorAccount->Available + pRspInvestorAccount->ShortMargin + pRspInvestorAccount->LongMargin;
	
	if( true == bIsLast )
	{
		lmice_info_print("get investor postion\n");
		sleep(1);
		investorposition(this);
	}
	
}

void CFemas2TraderSpi::OnRspQryInvestorPosition(CUstpFtdcRspInvestorPositionField *pRspInvestorPosition, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (pRspInfo!=NULL&&pRspInfo->ErrorID!=0)
    {
        lmice_error_print("Logout failed(%s0 as error id[%d]\n",gbktoutf8( pRspInfo->ErrorMsg).c_str(),
                          pRspInfo->ErrorID );
        m_state = FMTRADER_CONNECTED;
        return;
    }
	lmice_info_print("OnRspQryInvestorPosition\n");


	if( pRspInvestorPosition == NULL )
	{
		printf("==== NULL pRspInvestorPosition ==\n");
	}
	else
	{
		printf("Request ID:%d\n", nRequestID);
		printf("BrokerID:%s\n", pRspInvestorPosition->BrokerID);
		printf("InvestorID:%s\n", pRspInvestorPosition->InvestorID);
		printf("Direction:%c\n", pRspInvestorPosition->Direction);
		printf("HedgeFlag:%c\n", pRspInvestorPosition->HedgeFlag);
		printf("UsedMargin:%lf\n", pRspInvestorPosition->UsedMargin);
		printf("Position:%d\n", pRspInvestorPosition->Position);
		printf("PositionCost:%lf\n", pRspInvestorPosition->PositionCost);
		printf("YdPosition:%d\n", pRspInvestorPosition->YdPosition);
		printf("YdPositionCost:%lf\n", pRspInvestorPosition->YdPositionCost);
		printf("FrozenMargin:%lf\n", pRspInvestorPosition->FrozenMargin);
		printf("FrozenPosition:%d\n", pRspInvestorPosition->FrozenPosition);
		printf("FrozenClosing:%d\n", pRspInvestorPosition->FrozenClosing);
		printf("FrozenPremium:%lf\n", pRspInvestorPosition->FrozenPremium);
		printf("LastTradeID:%s\n", pRspInvestorPosition->LastTradeID);
		printf("LastOrderLocalID:%s\n", pRspInvestorPosition->LastOrderLocalID);
		printf("Currency:%s\n", pRspInvestorPosition->Currency);
		
		if( USTP_FTDC_D_Buy == pRspInvestorPosition->Direction )
		{
            g_cur_status.m_pos.m_yd_buy_pos = pRspInvestorPosition->YdPosition;
            g_cur_status.m_pos.m_buy_pos = pRspInvestorPosition->Position;
		}
		else if( USTP_FTDC_D_Sell == pRspInvestorPosition->Direction )
		{
            g_cur_status.m_pos.m_yd_sell_pos = pRspInvestorPosition->YdPosition;
            g_cur_status.m_pos.m_sell_pos = pRspInvestorPosition->Position;
		}

	}

	//update current status to stragegyht
	if( bIsLast )
	{
		//to be add,add trade insturment message and update to st
        g_cur_status.m_md.fee_rate = 0.000101 + 0.0000006;
        g_cur_status.m_md.m_up_price = 2814;
        g_cur_status.m_md.m_down_price = 2495;
        g_cur_status.m_md.m_last_price = 0;
        g_cur_status.m_md.m_multiple = 10;

		char symbol[32];
		MODELNAME(symbol, m_model_name, "Account");
		lmice_info_print("update account status to strategyht,flag:%s\n", symbol);
        //send( symbol, &g_cur_status, sizeof(CUR_STATUS));
	}
	
}

/*
/// time func
void time_func(int signo)
{
//	if( g_order_status != '0' && g_order_status != '5')
//	if( g_order_status != '0' )
	{
		orderaction( g_spi, &g_order_action);
		g_order_action_count++;
	}
}
*/

void CFemas2TraderSpi::OnRspOrderInsert(CUstpFtdcInputOrderField *pInputOrder, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    lmice_info_print("RspOrder insert ");

    if (pInputOrder==NULL )
    {
        printf("failed\n");
        return ;
    }

    if( pRspInfo != NULL )
    {
        printf("id:%d\treason:%s\n", pRspInfo->ErrorID,
               gbktoutf8(pRspInfo->ErrorMsg).c_str());
    }

}

void CFemas2TraderSpi::OnRspOrderAction(CUstpFtdcOrderActionField *pOrderAction, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{

	if( pRspInfo->ErrorID == 0 )
	{
		g_order_action_count++ ;
	}

    lmice_info_print("RspOrder action\n");
	if( pRspInfo != NULL )
    {
        printf("id:%d\treason:%s\n", pRspInfo->ErrorID,
               gbktoutf8(pRspInfo->ErrorMsg).c_str());
    }

}

void CFemas2TraderSpi::OnRspError(CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    lmice_info_print("OnRspError ");
	
	if( pRspInfo != NULL )
	{
        printf("id:%d\treq id:%d\treason:%s\n", pRspInfo->ErrorID
               , nRequestID
               , gbktoutf8(pRspInfo->ErrorMsg).c_str());
	}

}

void CFemas2TraderSpi::OnRtnTrade(CUstpFtdcTradeField *pTrade) 
{
	int update_flag = 0;

	printf("====== OnRtnTrade start ===\n direction:%c, option:%c\ng_active_buy_orders=%d\tg_active_sell_orders=%d\nm_buy_pos=%d\tm_sell_pos=%d\n\n", 
			pTrade->Direction, pTrade->OffsetFlag,
			g_active_buy_orders, g_active_sell_orders,
			g_cur_status.m_pos.m_buy_pos, g_cur_status.m_pos.m_sell_pos);

	if( USTP_FTDC_OF_Open == pTrade->OffsetFlag )
	{
		if( USTP_FTDC_D_Buy == pTrade->Direction ) 
		{
            g_cur_status.m_pos.m_buy_pos += pTrade->TradeVolume;
            g_cur_status.m_acc.m_left_cash -=
                    pTrade->TradeVolume * g_cur_status.m_md.m_multiple *
                    pTrade->TradePrice * ( 1 + g_cur_status.m_md.fee_rate );
			g_trade_buy_price = pTrade->TradePrice;
			//if(  )
			//g_active_buy_orders -= pTrade->TradeVolume;
		}
		else if( USTP_FTDC_D_Sell == pTrade->Direction )
		{
            g_cur_status.m_pos.m_sell_pos += pTrade->TradeVolume;
            g_cur_status.m_acc.m_left_cash +=
                    pTrade->TradeVolume * g_cur_status.m_md.m_multiple *
                    pTrade->TradePrice * ( 1 - g_cur_status.m_md.fee_rate );
			g_trade_sell_price = pTrade->TradePrice;
			//g_active_sell_orders -= pTrade->TradeVolume;
		}
		update_flag = 1;
	}
	else if( USTP_FTDC_OF_CloseToday == pTrade->OffsetFlag )
	{
		if( USTP_FTDC_D_Buy == pTrade->Direction ) 
		{
            g_cur_status.m_pos.m_sell_pos -= pTrade->TradeVolume;
            g_cur_status.m_acc.m_left_cash -=
                    pTrade->TradeVolume * g_cur_status.m_md.m_multiple *
                    pTrade->TradePrice * ( 1 + g_cur_status.m_md.fee_rate );
			//g_active_buy_orders -= pTrade->TradeVolume;
		}
		else if( USTP_FTDC_D_Sell == pTrade->Direction )
		{
            g_cur_status.m_pos.m_buy_pos -= pTrade->TradeVolume;
            g_cur_status.m_acc.m_left_cash +=
                    pTrade->TradeVolume * g_cur_status.m_md.m_multiple *
                    pTrade->TradePrice * ( 1 - g_cur_status.m_md.fee_rate );
			//g_active_sell_orders -= pTrade->TradeVolume;
		}
		update_flag = 1;
	}
	else if( USTP_FTDC_OF_CloseYesterday == pTrade->OffsetFlag )
	{
		if( USTP_FTDC_D_Buy == pTrade->Direction ) 
		{
            g_cur_status.m_pos.m_yd_buy_pos -= pTrade->TradeVolume;
		}
		else if( USTP_FTDC_D_Sell == pTrade->Direction )
		{
            g_cur_status.m_pos.m_yd_sell_pos -= pTrade->TradeVolume;
		}
	}
	else
	{
		lmice_error_print( "unknow offsetflag\n" );
	}

    if( update_flag )
    {
        int left_pos = g_cur_st->m_pos.m_buy_pos - g_cur_st->m_pos.m_sell_pos;
        double fee =  fabs(left_pos) * g_cur_st->m_md.m_multiple * g_cur_st->m_md.m_last_price * g_cur_st->m_md.fee_rate;
        double pl = g_cur_st->m_acc.m_left_cash + left_pos * g_cur_st->m_md.m_multiple * g_cur_st->m_md.m_last_price - fee;

        if( -pl >= g_conf->m_max_loss )
        {
            flatten_all(0, 0, 0);
            lmice_critical_print("touch max loss[%lf],stop and exit\n", pl);
        }

		printf("=== OnRtnTrade end ===\n g_active_buy_orders=%d\tg_active_sell_orders=%d\nm_buy_pos=%d\tm_sell_pos=%d\n\n", 
			g_active_buy_orders, g_active_sell_orders,
			g_cur_status.m_pos.m_buy_pos, g_cur_status.m_pos.m_sell_pos);
    }


}

void CFemas2TraderSpi::OnRtnOrder(CUstpFtdcOrderField *pOrder)
{
	int64_t systime = 0;
	get_system_time(&systime);
	g_end_time = systime;

	
	printf(" === order rtn	msg  start ===\n OrderSysID=%s\t UserOrderLocalID=%s OrderStatus=%c volume=%d leftvolume=%d \n timer time= %ld\n\n ",
			 pOrder->OrderSysID,  pOrder->UserOrderLocalID, pOrder->OrderStatus, pOrder->Volume, pOrder->VolumeRemain,
			 g_end_time);

	if( pOrder->OrderStatus != USTP_FTDC_OS_Canceled )
	{
		if( pOrder->Direction == USTP_FTDC_D_Buy )
		{
			g_active_buy_orders = pOrder->VolumeRemain;
			g_order_buy_price = pOrder->LimitPrice;
			g_order_sell_price = 0;
		}
		else 
		{
			g_active_sell_orders = pOrder->VolumeRemain;
			g_order_sell_price = pOrder->LimitPrice;
			g_order_buy_price = 0;
		}
	}
	
	else
	{
		if( pOrder->Direction == USTP_FTDC_D_Buy )
		{
			g_order_buy_price = 0;
			g_active_buy_orders = 0;		
		}
		else
		{
			g_order_sell_price = 0;
			g_active_sell_orders = 0;
		}
	}
	
	memset( g_cur_sysid, 0, sizeof(g_cur_sysid) );
	strcpy( g_cur_sysid, pOrder->OrderSysID );
	printf(" === order rtn  msg  end ===\n OrderSysID=%s\t UserOrderLocalID=%s OrderStatus=%c active_buy=%d active_sell=%d \n timer time= %ld\n\n ",
		 pOrder->OrderSysID,  pOrder->UserOrderLocalID, pOrder->OrderStatus, g_active_buy_orders, g_active_sell_orders,
		 g_end_time);

}

void CFemas2TraderSpi::OnRtnInstrumentStatus(CUstpFtdcInstrumentStatusField *pInstrumentStatus) {
lmice_info_print("OnRtnInstrumentStatus\n");
}
void CFemas2TraderSpi::OnRspQryInstrument(CUstpFtdcRspInstrumentField *p, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
    lmice_info_print("OnRspQryInstrument:%s m=%d, h=%lf, l=%lf, t=%lf\n", p->InstrumentID, p->VolumeMultiple,
        p->UpperLimitPrice, p->LowerLimitPrice, p->PriceTick);
    g_cur_status.m_md.m_multiple = p->VolumeMultiple;
    g_cur_status.m_md.m_down_price = p->LowerLimitPrice;
    g_cur_status.m_md.m_up_price = p->UpperLimitPrice;
	g_cur_status.m_md.m_price_tick = p->PriceTick;
    sleep(1);
    CUstpFtdcQryInvestorFeeField fee;
    strcpy(fee.BrokerID, broker_id());
    strcpy(fee.ExchangeID, exchange_id());
    strcpy(fee.InstrumentID, trading_instrument);
    strcpy(fee.InvestorID, investor_id());
    strcpy(fee.UserID, user_id());
    trader()->ReqQryInvestorFee(&fee, req_id());
    lmice_info_print("do ReqQryInvestorFee\n");
}

void CFemas2TraderSpi::OnRspQryInvestorFee(CUstpFtdcInvestorFeeField *p, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    //  ///开仓手续费按比例
    //    TUstpFtdcRatioType	OpenFeeRate;
    //	///开仓手续费按手数
    //	TUstpFtdcRatioType	OpenFeeAmt;
    //	///平仓手续费按比例
    //	TUstpFtdcRatioType	OffsetFeeRate;
    //	///平仓手续费按手数
    //	TUstpFtdcRatioType	OffsetFeeAmt;
    //	///平今仓手续费按比例
    //	TUstpFtdcRatioType	OTFeeRate;
    //	///平今仓手续费按手数
    //	TUstpFtdcRatioType	OTFeeAmt;
    lmice_info_print("OnRspQryInvestorFee:%s OpenFeeRate=%.8lf OpenFeeAmt=%.8lf OffsetFeeRate=%.8lf OffsetFeeAmt=%.8lf"
                     "OTFeeRate=%.8lf, OTFeeAmt=%.8lf\n", p->InstrumentID, p->OpenFeeRate,
                     p->OpenFeeAmt, p->OffsetFeeRate, p->OffsetFeeAmt,
                     p->OTFeeRate, p->OTFeeAmt
                     );
    g_cur_status.m_md.fee_rate = p->OTFeeRate;
}

void CFemas2TraderSpi::OnRtnInvestorAccountDeposit(CUstpFtdcInvestorAccountDepositResField *pInvestorAccountDepositRes) {

}

void CFemas2TraderSpi::OnErrRtnOrderInsert(CUstpFtdcInputOrderField *pInputOrder, CUstpFtdcRspInfoField *pRspInfo) {
	lmice_info_print("OnErrRtnOrderInsert\n");
    if( pRspInfo != NULL )
    {
        printf("id:%d\treason:%s\n", pRspInfo->ErrorID,
               gbktoutf8(pRspInfo->ErrorMsg).c_str());
    }
}

void CFemas2TraderSpi::OnErrRtnOrderAction(CUstpFtdcOrderActionField *pOrderAction, CUstpFtdcRspInfoField *pRspInfo) {
	lmice_info_print("OnErrRtnOrderAction\n");
    if( pRspInfo != NULL )
    {
        printf("id:%d\treason:%s\n", pRspInfo->ErrorID,
               gbktoutf8(pRspInfo->ErrorMsg).c_str());
    }
}
