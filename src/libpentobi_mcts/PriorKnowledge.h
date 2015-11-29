//-----------------------------------------------------------------------------
/** @file libpentobi_mcts/PriorKnowledge.h
    @author Markus Enzenberger
    @copyright GNU General Public License version 3 or later */
//-----------------------------------------------------------------------------

#ifndef LIBPENTOBI_MCTS_PRIOR_KNOWLEDGE_H
#define LIBPENTOBI_MCTS_PRIOR_KNOWLEDGE_H

#include "Float.h"
#include "SearchParamConst.h"
#include "libboardgame_mcts/Tree.h"
#include "libpentobi_base/Board.h"

namespace libpentobi_mcts {

using namespace std;
using libpentobi_base::Board;
using libpentobi_base::ColorMap;
using libpentobi_base::GridExt;
using libpentobi_base::Move;
using libpentobi_base::MoveList;
using libpentobi_base::PointList;

//-----------------------------------------------------------------------------

/** Initializes newly created nodes with heuristic prior count and value. */
class PriorKnowledge
{
public:
    typedef libboardgame_mcts::Node<Move, Float, SearchParamConst::multithread>
    Node;

    typedef libboardgame_mcts::Tree<Node> Tree;

    PriorKnowledge();

    void start_search(const Board& bd);

    /** Generate children nodes initialized with prior knowledge.
        @return false If the tree has not enough capacity for the children. */
    bool gen_children(const Board& bd, const MoveList& moves,
                      bool is_symmetry_broken, Tree::NodeExpander& expander,
                      Float init_val);

private:
    struct MoveFeatures
    {
        /** Heuristic value of the move expressed in score points. */
        Float heuristic;

        bool is_local;

        /** Does the move touch a piece of the same player? */
        bool connect;

        /** Only used on Classic and Trigon boards. */
        float dist_to_center;
    };

    array<MoveFeatures, Move::range> m_features;

    /** Maximum of Features::heuristic for all moves. */
    Float m_max_heuristic;

    bool m_has_connect_move;

    ColorMap<bool> m_check_dist_to_center;

    unsigned m_dist_to_center_max_pieces;

    float m_min_dist_to_center;

    float m_max_dist_diff;

    /** Marker for attach points of recent opponent moves. */
    GridExt<bool> m_is_local;

    /** Points in m_is_local with value greater zero. */
    PointList m_local_points;

    /** Distance to center heuristic. */
    GridExt<float> m_dist_to_center;

    void compute_features(const Board& bd, const MoveList& moves,
                          bool check_dist_to_center, bool check_connect);

    void init_local(const Board& bd);
};

//-----------------------------------------------------------------------------

} // namespace libpentobi_mcts

#endif // LIBPENTOBI_MCTS_PRIOR_KNOWLEDGE_H
