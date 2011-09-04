//-----------------------------------------------------------------------------
/** @file libpentobi_mcts/State.cpp */
//-----------------------------------------------------------------------------

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "State.h"

#include "libpentobi_base/BoardUtil.h"
#include "libboardgame_util/Log.h"

namespace libpentobi_mcts {

using namespace std;
using libboardgame_base::Geometry;
using libboardgame_base::Transform;
using libboardgame_mcts::Tree;
using libpentobi_base::game_variant_classic;
using libpentobi_base::game_variant_classic_2;
using libpentobi_base::game_variant_duo;
using libpentobi_base::BoardIterator;
using libpentobi_base::ColorIterator;
using libpentobi_base::ColorMove;
using libpentobi_base::Direction;
using libpentobi_base::FullGrid;
using libpentobi_base::GameVariant;
using libpentobi_base::MoveInfo;
using libpentobi_base::Point;
using libpentobi_base::PointState;
using libpentobi_base::PointStateExt;
using libboardgame_util::log;

//-----------------------------------------------------------------------------

namespace {

const bool log_simulations = false;

const bool use_prior_knowledge = true;

const bool pure_random_playout = false;

/** Return the symmetric point state for symmetry detection.
    Only used for game_variant_duo. Returns the other color or empty, if the
    given point state is empty. */
PointState get_symmetric_state(PointState s)
{
    if (s.is_empty())
        return s;
    Color c = s.to_color();
    LIBBOARDGAME_ASSERT(c.to_int() < 2); // game_variant_Duo
    return PointState(Color(1 - c.to_int()));
}

bool is_only_move_diag(const Board& bd, Point p, Color c, Move mv)
{
    LIBBOARDGAME_FOREACH_DIAG(p, p_diag,
        if (bd.get_point_state_ext(p_diag) == c
            && bd.get_played_move(p_diag) != mv)
            return false; )
    return true;
}

}

//-----------------------------------------------------------------------------

SharedConst::SharedConst(const Board& bd, const Color& to_play)
    : board(bd),
      to_play(to_play),
      detect_symmetry(true),
      avoid_symmetric_draw(true),
      score_modification(ValueType(0.1)),
      piece_value(bd)
{
}

//-----------------------------------------------------------------------------

State::State(const Board& bd, const SharedConst& shared_const)
  : m_shared_const(shared_const),
    m_bd(bd.get_game_variant())
{
}

State::State(const State& state)
  : m_shared_const(state.m_shared_const),
    m_bd(state.m_bd.get_game_variant())
{
}

State::~State() throw()
{
}

void State::check_local_move(int nu_local, Move mv)
{
    if (nu_local < m_max_local)
        return;
    if (nu_local > m_max_local)
    {
        m_max_local = nu_local;
        clear_local_moves();
    }
    m_local_moves.push_back(mv);
    m_local_move_marker.set(mv);
}

void State::clear_local_moves()
{
    m_local_move_marker.clear(m_local_moves);
    m_local_moves.clear();
}

void State::clear_local_points()
{
    BOOST_FOREACH(Point p, m_local_points)
        m_local_points_marker[p] = 0;
    m_local_points.clear();
}

void State::dump(ostream& out) const
{
    out << "pentobi_mcts::State:\n";
    libpentobi_base::boardutil::dump(m_bd, out);
}

array<ValueType, 4> State::evaluate_playout()
{
    if (m_check_symmetric_draw && ! m_is_symmetry_broken
        && m_bd.get_nu_moves() >= 3)
    {
        // Always evaluate symmetric positions as a draw in the playouts.
        // This will encourage the first player to break the symmetry and
        // the second player to preserve it. (Exception: don't do this if
        // the move number is less than 3 because the earliest time to break
        // the symmetry is move 3 and otherwise all moves are evaluated as
        // draw in very short searches.)
        if (log_simulations)
            log() << "Result: 0.5 (symmetry)\n";
        m_score_sum += 0;
        array<ValueType, 4> result;
        result[0] = result[1] = 0.5;
        return result;
    }
    return evaluate_terminal();
}

array<ValueType, 4> State::evaluate_terminal()
{
    array<ValueType, 4> result_array;
    for (ColorIterator i(m_bd.get_nu_colors()); i; ++i)
    {
        double game_result;
        int score = m_bd.get_score(*i, game_result);
        ValueType score_modification = m_shared_const.score_modification;
        ValueType result;
        if (game_result == 1)
            result =
                1 - score_modification + score * m_score_modification_factor;
        else if (game_result == 0)
            result = score_modification + score * m_score_modification_factor;
        else
            result = 0.5;
        if (*i == m_shared_const.to_play)
            m_score_sum += score;
        result_array[(*i).to_int()] = result;
        if (log_simulations)
            log() << "Result color " << (*i).to_int() << ": score=" << score
                  << " result=" << result << '\n';
    }
    return result_array;
}

bool State::gen_and_play_playout_move()
{
    unsigned int nu_colors = m_bd.get_nu_colors();
    if (m_nu_passes == nu_colors)
        return false;
    Color to_play = m_bd.get_to_play();
    GameVariant variant = m_bd.get_game_variant();
    Color to_play_second_color = to_play;
    if (variant == game_variant_classic_2)
        to_play_second_color = to_play.get_next(nu_colors).get_next(nu_colors);
    m_has_moves[to_play] = ! m_moves[to_play].empty();

    // Don't care about the exact score of a playout if we are still early in
    // the game and we know that the playout is a loss because the player has
    // no more moves and the score is already negative.
    if (! m_has_moves[to_play] && m_nu_moves_initial < 10 * nu_colors
        && (variant == game_variant_duo
            || (variant == game_variant_classic_2
                && ! m_has_moves[to_play_second_color])))
    {
        double game_result;
        if (m_bd.get_score(to_play, game_result) < 0)
        {
            if (log_simulations)
                log() << "Terminate early (no moves and negative score)\n";
            return false;
        }
    }

    if (m_check_symmetric_draw)
        if (! m_is_symmetry_broken && m_bd.get_nu_moves() >= 3)
        {
            // See also the comment in evaluate_playout()
            if (log_simulations)
                log() << "Terminate playout. Symmetry not broken.\n";
            return false;
        }
    const vector<Move>* moves;
    if (pure_random_playout)
        moves = &m_moves[to_play];
    else
    {
        if (log_simulations)
            log() << "Moves: " << m_moves[to_play].size() << ", local: "
                  << m_local_moves.size() << '\n';
        moves = &m_local_moves;
        if (moves->empty())
            moves = &m_moves[to_play];
    }
    if (moves->empty())
    {
        play(Move::pass());
        return true;
    }
    int i = m_random.generate_small_int(static_cast<int>(moves->size()));
    Move mv = (*moves)[i];
    play(mv);
    return true;
}

void State::gen_children(Tree<Move>::NodeExpander& expander)
{
    if (m_nu_passes == m_bd.get_nu_colors())
        return;
    Color to_play = m_bd.get_to_play();
    init_move_list(to_play);
    init_symmetry_info();
    m_extended_update = true;
    const vector<Move>& moves = m_moves[to_play];
    if (moves.empty())
    {
        expander.add_child(Move::pass());
        return;
    }
    if (! use_prior_knowledge)
    {
        BOOST_FOREACH(Move mv, moves)
            expander.add_child(mv);
        return;
    }
    Move symmetric_mv = Move::null();
    bool has_symmetry_breaker = false;
    if (m_check_symmetric_draw && ! m_is_symmetry_broken)
    {
        if (to_play == Color(1))
        {
            ColorMove last = m_bd.get_move(m_bd.get_nu_moves() - 1);
            symmetric_mv = m_bd.get_move_info(last.move).symmetric_move;
        }
        else if (m_bd.get_nu_moves() > 0)
        {
            BOOST_FOREACH(Move mv, moves)
                if (m_bd.get_move_info(mv).breaks_symmetry)
                {
                    has_symmetry_breaker = true;
                    break;
                }
        }
    }
    GameVariant variant = m_bd.get_game_variant();
    unsigned int min_dist_to_center = numeric_limits<unsigned int>::max();
    if ((variant == game_variant_classic_2 || variant == game_variant_classic)
        && m_bd.get_pieces_left(to_play).size() > Board::nu_pieces - 4)
    {
        // Determine moves that minimize the distance to the center to give a
        // bonus for reaching for the center in the early game.
        BOOST_FOREACH(Move mv, moves)
        {
            unsigned int dist = m_bd.get_move_info(mv).dist_to_center;
            min_dist_to_center = min(dist, min_dist_to_center);
        }
    }
    BOOST_FOREACH(Move mv, moves)
    {
        const MoveInfo& info = m_bd.get_move_info(mv);
        // Even game heuristic (0.5) with small piece value bonus to order the
        // values by piece value
        ValueType value =
            ValueType(0.5 + 0.01 * m_shared_const.piece_value.get(info.piece));
        ValueType count = 1;
        if (! m_local_moves.empty())
        {
            if (m_local_move_marker[mv])
                value += 3 * ValueType(0.9);
            else
                value += 3 * ValueType(0.5);
            count += 3;
        }
        if (min_dist_to_center != numeric_limits<unsigned int>::max())
        {
            if (info.dist_to_center == min_dist_to_center)
                value += 5 * ValueType(0.9);
            else
                value += 5 * ValueType(0.1);
            count += 5;
        }
        // Encourage to explore a move that keeps or breaks symmetry
        // See also the comment in evaluate_playout()
        if (m_check_symmetric_draw && ! m_is_symmetry_broken)
        {
            if (to_play == Color(1))
            {
                if (mv == symmetric_mv)
                    value += 5 * ValueType(1.0);
                else
                    value += 5 * ValueType(0.1);
                count += 5;
            }
            else if (has_symmetry_breaker)
            {
                if (m_bd.get_move_info(mv).breaks_symmetry)
                    value += 5 * ValueType(1.0);
                else
                    value += 5 * ValueType(0.1);
                count += 5;
            }
        }
        if (count > 0)
            value /= count;
        expander.add_child(mv, value, count, value, count);
    }
}

void State::init_local_points()
{
    LIBBOARDGAME_ASSERT(m_local_points.empty());
    Color to_play = m_bd.get_to_play();
    unsigned int nu_colors = m_bd.get_nu_colors();
    Color to_play_second_color = to_play;
    if (m_bd.get_game_variant() == game_variant_classic_2)
        to_play_second_color = to_play.get_next(nu_colors).get_next(nu_colors);
    unsigned int move_number = m_bd.get_nu_moves();
    // Consider last 3 moves for local points (i.e. last 2 opponent moves in
    // two-player variants)
    for (unsigned int i = 0; i < 3; ++i)
    {
        if (move_number == 0)
            return;
        --move_number;
        ColorMove move = m_bd.get_move(move_number);
        Color c = move.color;
        if (c == to_play || c == to_play_second_color)
            continue;
        Move mv = move.move;
        if (mv.is_pass())
            continue;
        BOOST_FOREACH(Point p, m_bd.get_move_info(mv).corner_points)
            if (! m_bd.is_forbidden(c, p))
            {
                m_local_points.push_back(p);
                m_local_points_marker[p] = 1;
            }
    }
}

void State::init_move_list(Color c)
{
    vector<Move>& moves = m_moves[c];
    m_bd.gen_moves(c, moves);
    m_last_move[c] = Move::null();
    init_local_points();
    clear_local_moves();
    m_max_local = 1;
    for (auto i = moves.begin(); i != moves.end(); ++i)
    {
        const MovePoints& points = m_bd.get_move_points(*i);
        int nu_local = 0;
        for (auto j = points.begin(); j != points.end(); ++j)
            nu_local += m_local_points_marker[*j];
        check_local_move(nu_local, *i);
    }
    clear_local_points();
    m_is_move_list_initialized[c] = true;
}

void State::init_symmetry_info()
{
    m_is_symmetry_broken = false;
    if (m_bd.get_to_play() == Color(0))
    {
        // First player to play: We set m_is_symmetry_broken to true if the
        // position is symmetric.
        for (BoardIterator i(m_bd); i; ++i)
        {
            PointState s1 = m_bd.get_point_state(*i);
            PointState s2 = m_bd.get_point_state(m_symmetric_points[*i]);
            if (s1 != get_symmetric_state(s2))
            {
                m_is_symmetry_broken = true;
                return;
            }
        }
    }
    else
    {
        // Second player to play: We set m_is_symmetry_broken to true if the
        // second player cannot copy thr first player's last move to make the
        // position symmetric again.
        unsigned int nu_moves = m_bd.get_nu_moves();
        if (nu_moves == 0)
        {
            // Don't try to handle white to play as first move
            m_is_symmetry_broken = true;
            return;
        }
        ColorMove last_mv = m_bd.get_move(nu_moves - 1);
        if (last_mv.color != Color(0))
        {
            // Don't try to handle non-alternating moves in board history
            m_is_symmetry_broken = true;
            return;
        }
        const MovePoints* points;
        if (last_mv.move.is_pass())
            points = 0;
        else
            points = &m_bd.get_move_points(last_mv.move);
        for (BoardIterator i(m_bd); i; ++i)
        {
            Point sym_p = m_symmetric_points[*i];
            PointState s1 = m_bd.get_point_state(*i);
            PointState s2 = m_bd.get_point_state(sym_p);
            if (s1 != get_symmetric_state(s2))
            {
                if (points != 0)
                {
                    if ((points->contains(*i)
                         && s1 == Color(0) && s2.is_empty())
                        || (points->contains(sym_p)
                            && s1.is_empty() && s2 == Color(0)))
                        continue;
                }
                m_is_symmetry_broken = true;
                return;
            }
        }
    }
}

bool State::is_forbidden(Color c, const MovePoints& points, int& nu_local) const
{
    nu_local = 0;
    const FullGrid<bool>& is_forbidden = m_bd.is_forbidden(c);
    for (auto i = points.begin(); i != points.end(); ++i)
    {
        if (is_forbidden[*i])
            return true;
        nu_local += m_local_points_marker[*i];
    }
    return false;
}

void State::play(const Move& mv)
{
    m_last_move[m_bd.get_to_play()] = mv;
    m_bd.play(mv);
    if (mv.is_pass())
        ++m_nu_passes;
    else
        m_nu_passes = 0;
    if (m_extended_update)
    {
        Color to_play = m_bd.get_to_play();
        if (m_is_move_list_initialized[to_play])
            update_move_list(to_play);
        else
            init_move_list(to_play);
        update_symmetry_info(mv);
    }
    if (log_simulations)
        log() << m_bd;
}

void State::start_playout()
{
    if (log_simulations)
        log() << "Start playout\n";
    if (! m_extended_update)
    {
        init_move_list(m_bd.get_to_play());
        init_symmetry_info();
        m_extended_update = true;
    }
}

void State::start_search()
{
    const Board& bd = m_shared_const.board;
    unsigned int sz = bd.get_size();
    m_symmetric_points.init(sz);
    m_local_points_marker.init(sz, 0);
    m_nu_moves_initial = bd.get_nu_moves();
    m_score_modification_factor =
        m_shared_const.score_modification / BoardConst::total_piece_points;
    m_nu_simulations = 0;
    m_score_sum = 0;
    m_check_symmetric_draw =
        (bd.get_game_variant() == game_variant_duo
         && m_shared_const.detect_symmetry
         && ! (m_shared_const.to_play == Color(1)
               && m_shared_const.avoid_symmetric_draw));
}

void State::start_simulation(size_t n)
{
    if (log_simulations)
        log() << "==========================================================\n"
              << "Simulation " << n << "\n"
              << "==========================================================\n";
    ++m_nu_simulations;
    m_bd.copy_from(m_shared_const.board);
    m_bd.set_to_play(m_shared_const.to_play);
    m_extended_update = false;
    for (ColorIterator i(m_bd.get_nu_colors()); i; ++i)
    {
        m_has_moves[*i] = true;
        m_is_move_list_initialized[*i] = false;
    }
    m_nu_passes = 0;
    // TODO: m_nu_passes should be initialized without asuming alternating
    // colors in the board's move history
    for (unsigned int i = m_bd.get_nu_moves(); i > 0; --i)
    {
        if (! m_bd.get_move(i - 1).move.is_pass())
            break;
        ++m_nu_passes;
    }
}

void State::update_move_list(Color c)
{
    init_local_points();
    m_tmp_moves.clear();
    m_local_move_marker.clear(m_local_moves);
    m_local_moves.clear();
    m_max_local = 1;
    Move last_mv = m_last_move[c];

    // Find old moves that are still legal
    unsigned int last_piece = numeric_limits<unsigned int>::max();
    if (! last_mv.is_null() && ! last_mv.is_pass())
        last_piece = m_bd.get_move_info(last_mv).piece;
    for (auto i = m_moves[c].begin(); i != m_moves[c].end(); ++i)
    {
        int nu_local;
        const MoveInfo& info = m_bd.get_move_info(*i);
        if (info.piece != last_piece
            && ! is_forbidden(c, info.points, nu_local))
        {
            m_tmp_moves.push_back(*i);
            m_marker.set(*i);
            check_local_move(nu_local, *i);
        }
    }

    // Find new legal moves because of the last piece played by this color
    if (! last_mv.is_null() && ! last_mv.is_pass())
    {
        BOOST_FOREACH(Point p, m_bd.get_move_points(last_mv))
            for (unsigned int i = 0; i < 4; ++i)
            {
                Direction dir = Direction::get_enum_diag(i);
                Point diag = p.get_neighbor(dir);
                if (! m_bd.is_forbidden(c, diag)
                    && is_only_move_diag(m_bd, diag, c, last_mv))
                {
                    unsigned int diag_dir = Direction::get_index_diag_inv(i);
                    BOOST_FOREACH(unsigned int i, m_bd.get_pieces_left(c))
                    {
                        const vector<Move>& moves =
                            m_bd.get_moves_diag(i, diag, diag_dir);
                        auto begin = moves.begin();
                        auto end = moves.end();
                        for (auto i = begin; i != end; ++i)
                        {
                            int nu_local;
                            const MoveInfo& info = m_bd.get_move_info(*i);
                            if (! is_forbidden(c, info.points, nu_local)
                                && ! m_marker[*i])
                            {
                                m_tmp_moves.push_back(*i);
                                m_marker.set(*i);
                                check_local_move(nu_local, *i);
                            }
                        }
                    }
                }
            }
    }

    swap(m_moves[c], m_tmp_moves);

    m_marker.clear(m_moves[c]);
    clear_local_points();
    m_last_move[c] = Move::null();
}

void State::update_symmetry_info(Move mv)
{
    if (m_is_symmetry_broken)
        return;
    if (mv.is_pass())
    {
        // Don't try to handle pass moves: a pass move either breaks symmetry
        // or both players have passed and it's the end of the game and
        // we need symmetry detection only as a heuristic (playouts and
        // move value initialization)
        m_is_symmetry_broken = true;
        return;
    }
    const MovePoints& points = m_bd.get_move_points(mv);
    if (m_bd.get_to_play() == Color(0))
    {
        // First player to play: Check that all symmetric points of the last
        // move of the second player are occupied by the first player
        for (auto i = points.begin(); i != points.end(); ++i)
            if (m_bd.get_point_state(m_symmetric_points[*i]) != Color(0))
            {
                m_is_symmetry_broken = true;
                return;
            }
    }
    else
    {
        // Second player to play: Check that all symmetric points of the last
        // move of the first player are empty (i.e. the second can play there
        // to preserve the symmetry)
        for (auto i = points.begin(); i != points.end(); ++i)
            if (! m_bd.get_point_state(m_symmetric_points[*i]).is_empty())
            {
                m_is_symmetry_broken = true;
                return;
            }
    }
}

//-----------------------------------------------------------------------------

} // namespace libpentobi_mcts
