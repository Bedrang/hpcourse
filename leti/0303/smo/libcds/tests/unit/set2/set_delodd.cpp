//$$CDS-header$$

#include "cppunit/thread.h"
#include "set2/set_types.h"
#include <algorithm> // random_shuffle

namespace set2 {

#    define TEST_SET(X)         void X() { test<SetTypes<key_type, value_type>::X >()    ; }
#    define TEST_SET_NOLF(X)    void X() { test_nolf<SetTypes<key_type, value_type>::X >()    ; }

    namespace {
        static size_t  c_nSetSize = 1000000         ;  // max set size
        static size_t  c_nInsThreadCount = 4        ;  // insert thread count
        static size_t  c_nDelThreadCount = 4        ;  // delete thread count
        static size_t  c_nMaxLoadFactor = 8         ;  // maximum load factor
        static bool    c_bPrintGCState = true       ;
    }

    namespace {
        struct key_thread
        {
            size_t  nKey    ;
            size_t  nThread ;

            key_thread( size_t key, size_t threadNo )
                : nKey( key )
                , nThread( threadNo )
            {}

            key_thread()
            {}
        };

        typedef SetTypes<key_thread, size_t>::key_val     key_value_pair    ;
    }

    template <>
    struct cmp<key_thread> {
        int operator ()(key_thread const& k1, key_thread const& k2) const
        {
            if ( k1.nKey < k2.nKey )
                return -1   ;
            if ( k1.nKey > k2.nKey )
                return 1    ;
            if ( k1.nThread < k2.nThread )
                return -1   ;
            if ( k1.nThread > k2.nThread )
                return 1    ;
            return 0        ;
        }
        int operator ()(key_thread const& k1, size_t k2) const
        {
            if ( k1.nKey < k2 )
                return -1   ;
            if ( k1.nKey > k2 )
                return 1    ;
            return 0        ;
        }
        int operator ()(size_t k1, key_thread const& k2) const
        {
            if ( k1 < k2.nKey )
                return -1   ;
            if ( k1 > k2.nKey )
                return 1    ;
            return 0        ;
        }
    };

} // namespace set2

namespace std {
    template <>
    struct less<set2::key_thread>
    {
        bool operator()(set2::key_thread const& k1, set2::key_thread const& k2) const
        {
            if ( k1.nKey <= k2.nKey )
                return k1.nKey < k2.nKey || k1.nThread < k2.nThread ;
            return false    ;
        }
    };
} // namespace std

CDS_BEGIN_STD_HASH_NAMESPACE
    template <>
    struct hash<set2::key_thread>
    {
        typedef size_t              result_type     ;
        typedef set2::key_thread    argument_type   ;

        size_t operator()(set2::key_thread const& k) const
        {
            return CDS_STD_HASH_NAMESPACE::hash<size_t>()( k.nKey ) ;
        }
        size_t operator()(size_t k) const
        {
            return CDS_STD_HASH_NAMESPACE::hash<size_t>()( k ) ;
        }
    };
CDS_END_STD_HASH_NAMESPACE

namespace boost {
    inline size_t hash_value( set2::key_thread const& k )
    {
        return CDS_STD_HASH_NAMESPACE::hash<size_t>()( k.nKey ) ;
    }

    template <>
    struct hash<set2::key_thread>
    {
        typedef size_t              result_type     ;
        typedef set2::key_thread    argument_type   ;

        size_t operator()(set2::key_thread const& k) const
        {
            return boost::hash<size_t>()( k.nKey ) ;
        }
        size_t operator()(size_t k) const
        {
            return boost::hash<size_t>()( k ) ;
        }
    };
} // namespace boost


namespace set2 {

    template <typename Set>
    static inline void check_before_clear( Set& s )
    {}

    template <typename GC, typename Key, typename T, typename Traits>
    static inline void check_before_clear( cds::container::EllenBinTreeSet<GC, Key, T, Traits>& s )
    {
        CPPUNIT_CHECK_CURRENT( s.check_consistency() ) ;
    }

    class Set_DelOdd: public CppUnitMini::TestCase
    {
        std::vector<size_t>     m_arrData ;

    protected:
        typedef key_thread  key_type    ;
        typedef size_t      value_type  ;

        CDS_ATOMIC::atomic<size_t>      m_nInsThreadCount ;

        // Inserts keys from [0..N)
        template <class Set>
        class InsertThread: public CppUnitMini::TestThread
        {
            Set&     m_Set      ;

            virtual InsertThread *    clone()
            {
                return new InsertThread( *this )    ;
            }

            struct ensure_func
            {
                template <typename Q>
                void operator()( bool bNew, key_value_pair const&, Q const& )
                {}
            };
        public:
            size_t  m_nInsertSuccess    ;
            size_t  m_nInsertFailed     ;

        public:
            InsertThread( CppUnitMini::ThreadPool& pool, Set& rMap )
                : CppUnitMini::TestThread( pool )
                , m_Set( rMap )
            {}
            InsertThread( InsertThread& src )
                : CppUnitMini::TestThread( src )
                , m_Set( src.m_Set )
            {}

            Set_DelOdd&  getTest()
            {
                return reinterpret_cast<Set_DelOdd&>( m_Pool.m_Test )   ;
            }

            virtual void init() { cds::threading::Manager::attachThread()   ; }
            virtual void fini() { cds::threading::Manager::detachThread()   ; }

            virtual void test()
            {
                Set& rSet = m_Set   ;

                m_nInsertSuccess =
                    m_nInsertFailed = 0 ;

                std::vector<size_t>& arrData = getTest().m_arrData ;
                for ( size_t i = 0; i < arrData.size(); ++i ) {
                    if ( rSet.insert( key_type( arrData[i], m_nThreadNo )))
                        ++m_nInsertSuccess ;
                    else
                        ++m_nInsertFailed ;
                }

                ensure_func f ;
                for ( size_t i = arrData.size() - 1; i > 0; --i ) {
                    if ( arrData[i] & 1 ) {
                        rSet.ensure( key_type( arrData[i], m_nThreadNo ), f ) ;
                    }
                }

                getTest().m_nInsThreadCount.fetch_sub( 1, CDS_ATOMIC::memory_order_acquire ) ;
            }
        };

        // Deletes odd keys from [0..N)
        template <class Set>
        class DeleteThread: public CppUnitMini::TestThread
        {
            Set&     m_Set      ;

            virtual DeleteThread *    clone()
            {
                return new DeleteThread( *this )    ;
            }
        public:
            size_t  m_nDeleteSuccess    ;
            size_t  m_nDeleteFailed     ;


            struct key_equal {
                bool operator()( key_type const& k1, key_type const& k2 ) const
                {
                    return k1.nKey == k2.nKey ;
                }
                bool operator()( size_t k1, key_type const& k2 ) const
                {
                    return k1 == k2.nKey ;
                }
                bool operator()( key_type const& k1, size_t k2 ) const
                {
                    return k1.nKey == k2 ;
                }
                bool operator ()( key_value_pair const& k1, key_value_pair const& k2 ) const
                {
                    return operator()( k1.key, k2.key ) ;
                }
                bool operator ()( key_value_pair const& k1, key_type const& k2 ) const
                {
                    return operator()( k1.key, k2 ) ;
                }
                bool operator ()( key_type const& k1, key_value_pair const& k2 ) const
                {
                    return operator()( k1, k2.key ) ;
                }
                bool operator ()( key_value_pair const& k1, size_t k2 ) const
                {
                    return operator()( k1.key, k2 ) ;
                }
                bool operator ()( size_t k1, key_value_pair const& k2 ) const
                {
                    return operator()( k1, k2.key ) ;
                }
            };

            struct key_less {
                bool operator()( key_type const& k1, key_type const& k2 ) const
                {
                    return k1.nKey < k2.nKey ;
                }
                bool operator()( size_t k1, key_type const& k2 ) const
                {
                    return k1 < k2.nKey ;
                }
                bool operator()( key_type const& k1, size_t k2 ) const
                {
                    return k1.nKey < k2 ;
                }
                bool operator ()( key_value_pair const& k1, key_value_pair const& k2 ) const
                {
                    return operator()( k1.key, k2.key ) ;
                }
                bool operator ()( key_value_pair const& k1, key_type const& k2 ) const
                {
                    return operator()( k1.key, k2 ) ;
                }
                bool operator ()( key_type const& k1, key_value_pair const& k2 ) const
                {
                    return operator()( k1, k2.key ) ;
                }
                bool operator ()( key_value_pair const& k1, size_t k2 ) const
                {
                    return operator()( k1.key, k2 ) ;
                }
                bool operator ()( size_t k1, key_value_pair const& k2 ) const
                {
                    return operator()( k1, k2.key ) ;
                }

                typedef key_equal   equal_to ;
            };

        public:
            DeleteThread( CppUnitMini::ThreadPool& pool, Set& rMap )
                : CppUnitMini::TestThread( pool )
                , m_Set( rMap )
            {}
            DeleteThread( DeleteThread& src )
                : CppUnitMini::TestThread( src )
                , m_Set( src.m_Set )
            {}

            Set_DelOdd&  getTest()
            {
                return reinterpret_cast<Set_DelOdd&>( m_Pool.m_Test )   ;
            }

            virtual void init() { cds::threading::Manager::attachThread()   ; }
            virtual void fini() { cds::threading::Manager::detachThread()   ; }

            virtual void test()
            {
                Set& rSet = m_Set   ;

                m_nDeleteSuccess =
                    m_nDeleteFailed = 0 ;

                std::vector<size_t>& arrData = getTest().m_arrData ;
                if ( m_nThreadNo & 1 ) {
                    for ( size_t k = 0; k < c_nInsThreadCount; ++k ) {
                        for ( size_t i = 0; i < arrData.size(); ++i ) {
                            if ( arrData[i] & 1 ) {
                                if ( rSet.erase_with( arrData[i], key_less() ))
                                    ++m_nDeleteSuccess  ;
                                else
                                    ++m_nDeleteFailed   ;
                            }
                        }
                        if ( getTest().m_nInsThreadCount.load( CDS_ATOMIC::memory_order_acquire ) == 0 )
                            break;
                    }
                }
                else {
                    for ( size_t k = 0; k < c_nInsThreadCount; ++k ) {
                        for ( size_t i = arrData.size() - 1; i > 0; --i ) {
                            if ( arrData[i] & 1 ) {
                                if ( rSet.erase_with( arrData[i], key_less() ))
                                    ++m_nDeleteSuccess  ;
                                else
                                    ++m_nDeleteFailed   ;
                            }
                        }
                        if ( getTest().m_nInsThreadCount.load( CDS_ATOMIC::memory_order_acquire ) == 0 )
                            break;
                    }
                }
            }
        };

    protected:
        template <class Set>
        void do_test( size_t nLoadFactor )
        {
            Set  testSet( c_nSetSize, nLoadFactor ) ;
            do_test_with( testSet ) ;
        }

        template <class Set>
        void do_test_with( Set& testSet )
        {
            typedef InsertThread<Set> insert_thread ;
            typedef DeleteThread<Set> delete_thread ;

            m_nInsThreadCount.store( c_nInsThreadCount, CDS_ATOMIC::memory_order_release ) ;
            cds::OS::Timer    timer    ;

            CppUnitMini::ThreadPool pool( *this )   ;
            pool.add( new insert_thread( pool, testSet ), c_nInsThreadCount ) ;
            pool.add( new delete_thread( pool, testSet ), c_nDelThreadCount ) ;
            pool.run() ;
            CPPUNIT_MSG( "   Duration=" << pool.avgDuration() ) ;

            size_t nInsertSuccess = 0   ;
            size_t nInsertFailed = 0    ;
            size_t nDeleteSuccess = 0   ;
            size_t nDeleteFailed = 0    ;
            for ( CppUnitMini::ThreadPool::iterator it = pool.begin(); it != pool.end(); ++it ) {
                insert_thread * pThread = dynamic_cast<insert_thread *>( *it )   ;
                if ( pThread ) {
                    nInsertSuccess += pThread->m_nInsertSuccess ;
                    nInsertFailed += pThread->m_nInsertFailed   ;
                }
                else {
                    delete_thread * p = static_cast<delete_thread *>( *it ) ;
                    nDeleteSuccess += p->m_nDeleteSuccess   ;
                    nDeleteFailed += p->m_nDeleteFailed ;
                }
            }

            CPPUNIT_MSG( "  Totals (success/failed): \n\t"
                      << "      Insert=" << nInsertSuccess << '/' << nInsertFailed << "\n\t"
                      << "      Delete=" << nDeleteSuccess << '/' << nDeleteFailed << "\n\t"
            ) ;

            // All even keys must be in the set
            {
                CPPUNIT_MSG( "  Check even keys..." ) ;
                size_t nErrorCount = 0 ;
                for ( size_t n = 0; n < c_nSetSize; n +=2 ) {
                    for ( size_t i = 0; i < c_nInsThreadCount; ++i ) {
                        if ( !testSet.find( key_type(n, i) ) ) {
                            if ( ++nErrorCount < 10 ) {
                                CPPUNIT_MSG( "key " << n << "-" << i << " is not found!") ;
                            }
                        }
                    }
                }
                CPPUNIT_CHECK_EX( nErrorCount == 0, "Totals: " << nErrorCount << " keys is not found");
                //if ( nErrorCount >= 10 ) {
                //    CPPUNIT_MSG( "Totals: " << nErrorCount << " keys is not found") ;
                //}
            }

            check_before_clear( testSet )    ;

            CPPUNIT_CHECK( nInsertSuccess == c_nSetSize * c_nInsThreadCount ) ;
            CPPUNIT_CHECK( nInsertFailed == 0 ) ;

            CPPUNIT_MSG( "  Clear map (single-threaded)..." ) ;
            timer.reset()   ;
            testSet.clear() ;
            CPPUNIT_MSG( "   Duration=" << timer.duration() ) ;
            CPPUNIT_CHECK_EX( testSet.empty(), ((long long) testSet.size()) ) ;

            additional_check( testSet ) ;
            print_stat( testSet )       ;

            additional_cleanup( testSet ) ;
        }

        template <class Set>
        void test()
        {
            CPPUNIT_MSG( "Insert thread count=" << c_nInsThreadCount
                << " delete thread count=" << c_nDelThreadCount
                << " set size=" << c_nSetSize
                );

            for ( size_t nLoadFactor = 1; nLoadFactor <= c_nMaxLoadFactor; nLoadFactor *= 2 ) {
                CPPUNIT_MSG( "Load factor=" << nLoadFactor )   ;
                do_test<Set>( nLoadFactor )     ;
                if ( c_bPrintGCState )
                    print_gc_state()            ;
            }
        }

        template <class Set>
        void test_nolf()
        {
            CPPUNIT_MSG( "Insert thread count=" << c_nInsThreadCount
                << " delete thread count=" << c_nDelThreadCount
                << " set size=" << c_nSetSize
                );

            Set s ;
            do_test_with( s )     ;
            if ( c_bPrintGCState )
                print_gc_state()            ;
        }

        void setUpParams( const CppUnitMini::TestCfg& cfg ) {
            c_nSetSize = cfg.getULong("MapSize", 1000000 ) ;
            c_nInsThreadCount = cfg.getULong("InsThreadCount", 4 )  ;
            c_nDelThreadCount = cfg.getULong("DelThreadCount", 4 )  ;
            c_nMaxLoadFactor = cfg.getULong("MaxLoadFactor", 8 )    ;
            c_bPrintGCState = cfg.getBool("PrintGCStateFlag", true );

            if ( c_nInsThreadCount == 0 )
                c_nInsThreadCount = cds::OS::topology::processor_count() ;
            if ( c_nDelThreadCount == 0 )
                c_nDelThreadCount = cds::OS::topology::processor_count() ;

            m_arrData.resize( c_nSetSize ) ;
            for ( size_t i = 0; i < c_nSetSize; ++i )
                m_arrData[i] = i ;
            std::random_shuffle( m_arrData.begin(), m_arrData.end() )   ;
        }

#   include "set2/set_defs.h"
        CDSUNIT_DECLARE_MichaelSet
        CDSUNIT_DECLARE_SplitList
        //CDSUNIT_DECLARE_StripedSet
        //CDSUNIT_DECLARE_RefinableSet
        CDSUNIT_DECLARE_CuckooSet
        CDSUNIT_DECLARE_SkipListSet
        CDSUNIT_DECLARE_EllenBinTreeSet
        //CDSUNIT_DECLARE_StdSet

        CPPUNIT_TEST_SUITE_( Set_DelOdd, "Map_DelOdd" )
            CDSUNIT_TEST_MichaelSet
            CDSUNIT_TEST_SplitList
            CDSUNIT_TEST_SkipListSet
            CDSUNIT_TEST_EllenBinTreeSet
            //CDSUNIT_TEST_StripedSet
            //CDSUNIT_TEST_RefinableSet
            CDSUNIT_TEST_CuckooSet
            //CDSUNIT_TEST_StdSet
        CPPUNIT_TEST_SUITE_END()
    };

    CPPUNIT_TEST_SUITE_REGISTRATION( Set_DelOdd );
} // namespace set2
