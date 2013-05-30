/*

 Copyright (c) 2013, SMB Phone Inc.
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 The views and conclusions contained in the software and documentation are those
 of the authors and should not be interpreted as representing official policies,
 either expressed or implied, of the FreeBSD Project.

 */

#pragma once

#include <hookflash/core/internal/types.h>
#include <hookflash/core/IIdentityLookup.h>

#include <hookflash/stack/IBootstrappedNetwork.h>
#include <hookflash/stack/IMessageMonitor.h>
#include <hookflash/stack/message/identity-lookup/IdentityLookupResult.h>

#include <zsLib/MessageQueueAssociator.h>

namespace hookflash
{
  namespace core
  {
    namespace internal
    {
      using stack::IMessageMonitorPtr;
      using stack::IMessageMonitorResultDelegate;
      using stack::message::MessageResultPtr;
      using stack::message::identity_lookup::IdentityLookupResult;
      using stack::message::identity_lookup::IdentityLookupResultPtr;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IdentityLookup
      #pragma mark

      class IdentityLookup : public Noop,
                             public MessageQueueAssociator,
                             public IIdentityLookup,
                             public IBootstrappedNetworkDelegate,
                             public IMessageMonitorResultDelegate<IdentityLookupResult>
      {
      public:
        friend interaction IIdentityLookupFactory;
        friend interaction IIdentityLookup;

        typedef String Domain;
        typedef std::map<Domain, IBootstrappedNetworkPtr> BootstrappedNetworkMap;

        typedef String Identifier;
        typedef bool Bogus;
        typedef std::map<Identifier, Bogus> IdentifierMap;

        typedef String StringConcatinatedIdentifiers;

        typedef String DomainOrLegacyType;
        typedef std::map<DomainOrLegacyType, IdentifierMap> IdentifierDomainOrLegacyTypeMap;
        typedef std::map<DomainOrLegacyType, StringConcatinatedIdentifiers> IdentifierSafeCharDomainLegacyTypeMap;

        typedef std::map<Domain, bool> FailedBootstrappedNetworkDomainMap;

        typedef std::map<DomainOrLegacyType, Domain> DomainOrLegacyTypeToDomainMap;

        typedef PUID MonitorID;
        typedef std::map<MonitorID, IMessageMonitorPtr> MonitorMap;

      protected:
        IdentityLookup(
                       IMessageQueuePtr queue,
                       AccountPtr account,
                       IIdentityLookupDelegatePtr delegate
                       );
        
        IdentityLookup(Noop) : Noop(true), MessageQueueAssociator(IMessageQueuePtr()) {};

        void init(const IdentityURIList &identityURIs);

      public:
        ~IdentityLookup();

        static IdentityLookupPtr convert(IIdentityLookupPtr lookup);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IdentityLookup => IIdentityLookup
        #pragma mark

        static String toDebugString(IIdentityLookupPtr identity, bool includeCommaPrefix = true);

        static IdentityLookupPtr create(
                                        IAccountPtr account,
                                        IIdentityLookupDelegatePtr delegate,
                                        const IdentityURIList &identityURIs
                                        );

        virtual PUID getID() const {return mID;}

        virtual bool isComplete() const;
        virtual bool wasSuccessful(
                                   WORD *outErrorCode,
                                   String *outErrorReason
                                   ) const;

        virtual void cancel();

        virtual IdentityLookupInfoListPtr getIdentities() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IdentityLookup => IBootstrappedNetworkDelegate
        #pragma mark

        virtual void onBootstrappedNetworkPreparationCompleted(IBootstrappedNetworkPtr bootstrappedNetwork);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Identity => IMessageMonitorResultDelegate<IdentityLookupResult>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        IdentityLookupResultPtr result
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             IdentityLookupResultPtr ignore, // will always be NULL
                                                             MessageResultPtr result
                                                             );

      private:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IdentityLookup => (internal)
        #pragma mark

        String log(const char *message) const;
        virtual String getDebugValueString(bool includeCommaPrefix = true) const;

        RecursiveLock &getLock() const;

        void prepareIdentity(
                             const String &domain,
                             const String &type,
                             const String &identifier
                             );

        void step();

        void setError(WORD errorCode, const char *errorReason);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IdentityLookup => (data)
        #pragma mark

        PUID mID;
        IdentityLookupWeakPtr mThisWeak;

        AccountPtr mAccount;

        IIdentityLookupDelegatePtr mDelegate;

        String mLookupProviderDomain;
        bool mAlreadyIssuedForProviderDomain;

        WORD mErrorCode;
        String mErrorReason;

        BootstrappedNetworkMap mBootstrappedNetworks;
        MonitorMap mMonitors;

        IdentifierDomainOrLegacyTypeMap mDomainOrLegacyTypeIdentifiers;
        IdentifierSafeCharDomainLegacyTypeMap mConcatDomains;
        IdentifierSafeCharDomainLegacyTypeMap mSafeCharDomains;

        DomainOrLegacyTypeToDomainMap mTypeToDomainMap;

        IdentityLookupInfoList mResults;

        FailedBootstrappedNetworkDomainMap mFailedBootstrappedNetworks;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IIdentityLookupFactory
      #pragma mark

      interaction IIdentityLookupFactory
      {
        static IIdentityLookupFactory &singleton();

        virtual IdentityLookupPtr create(
                                         IAccountPtr account,
                                         IIdentityLookupDelegatePtr delegate,
                                         const IdentityURIList &identityURIs
                                         );
      };
    }
  }
}
