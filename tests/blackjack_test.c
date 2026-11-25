#include <assert.h>
#include <string.h>
#define main blackjack_application_main
#include "../games/blackjack.c"
#undef main

static void setupDeal(Game *game, int p1, int d1, int p2, int d2, int next)
{
    assert(resetGame(game));
    game->deck[0] = (Card){p1, 0};
    game->deck[1] = (Card){d1, 1};
    game->deck[2] = (Card){p2, 2};
    game->deck[3] = (Card){d2, 3};
    game->deck[4] = (Card){next, 0};
}

typedef struct {
    const uint32_t *values;
    int count;
    int index;
} Sequence;

static bool sequenceWord(uint32_t *value, void *context)
{
    Sequence *sequence = context;
    if (sequence->index >= sequence->count) { return(false); }
    *value = sequence->values[sequence->index++];
    return(true);
}

int main(void)
{
    for (int rank = 2; rank <= 10; rank++) {
        Vector2 positions[10] = {0};
        for (int i = 0; i < rank; i++) {
            assert(pipPosition(rank, i, &positions[i]));
            assert(positions[i].x >= -19.0f && positions[i].x <= 19.0f);
            assert(positions[i].y >= -32.0f && positions[i].y <= 32.0f);
            for (int j = 0; j < i; j++) { assert(positions[i].x != positions[j].x || positions[i].y != positions[j].y); }
        }
        assert(!pipPosition(rank, rank, &positions[0]));
    }
    assert(!pipPosition(1, 0, NULL));
    Vector2 pointer = blackjackPointer((Vector2){960, 540}, 1920, 1080);
    assert(pointer.x == 600.0f && pointer.y == 450.0f);
    pointer = blackjackPointer((Vector2){100, 540}, 1920, 1080);
    assert(pointer.x == -1.0f && pointer.y == -1.0f);
    assert(CheckCollisionPointRec((Vector2){250, 370}, blackjackActionRect(0)));
    assert(!CheckCollisionPointRec((Vector2){250, 370}, blackjackActionRect(1)));
    Vector2 compact = blackjackWindowSize(1280, 720, 1.0f, 1.0f);
    assert(compact.x == 864.0f && compact.y == 648.0f);
    Vector2 hidpi = blackjackWindowSize(1920, 1080, 2.0f, 2.0f);
    assert(hidpi.x == 648.0f && hidpi.y == 486.0f);
    Vector2 roomy = blackjackWindowSize(3840, 2160, 2.0f, 2.0f);
    assert(roomy.x == WIDTH && roomy.y == HEIGHT);
    Vector2 unknown = blackjackWindowSize(0, 0, 1.0f, 1.0f);
    assert(unknown.x == WIDTH && unknown.y == HEIGHT);
    Game game = {0};
    assert(resetGame(&game));
    const uint32_t samples[] = {0, 4, 5, 6};
    Sequence sequence = {samples, 4, 0};
    uint32_t value = 99;
    assert(randomBelow(10, sequenceWord, &sequence, &value));
    assert(value == 6 && sequence.index == 4);
    assert(!randomBelow(0, sequenceWord, &sequence, &value));
    Game previous = game;
    Sequence exhausted = {samples, 0, 0};
    assert(!shuffleWithRandom(&game, sequenceWord, &exhausted));
    assert(memcmp(&game, &previous, sizeof game) == 0);
    bool seen[4][14] = {{false}};
    for (int i = 0; i < DECK_SIZE; i++) {
        Card c = game.deck[i];
        assert(c.suit >= 0 && c.suit < 4 && c.rank >= 1 && c.rank <= 13 && !seen[c.suit][c.rank]);
        seen[c.suit][c.rank] = true;
    }
    Hand aces = {.cards = {{1, 0}, {1, 1}, {9, 2}}, .count = 3};
    assert(handValue(&aces) == 21 && !isBlackjack(&aces));
    aces.cards[aces.count++] = (Card){10, 1};
    assert(handValue(&aces) == 21);

    setupDeal(&game, 1, 10, 13, 9, 2);
    dealRound(&game);
    assert(game.phase == ROUND_OVER && game.result == PLAYER_BLACKJACK && game.chips == 1075);
    nextRound(&game);
    assert(game.phase == BETTING && game.chips == 1075);

    setupDeal(&game, 1, 1, 10, 13, 2);
    dealRound(&game);
    assert(game.phase == INSURANCE);
    insuranceChoice(&game, false);
    assert(game.phase == ROUND_OVER && game.result == PUSH && game.chips == 1000);
    setupDeal(&game, 10, 1, 9, 10, 2);
    dealRound(&game);
    assert(game.phase == INSURANCE);
    insuranceChoice(&game, false);
    assert(game.phase == ROUND_OVER && game.result == DEALER_WINS && game.chips == 950);

    setupDeal(&game, 10, 9, 8, 7, 10);
    dealRound(&game);
    assert(game.phase == PLAYER_TURN && game.chips == 950);
    hit(&game);
    assert(game.phase == ROUND_OVER && game.result == PLAYER_BUST && game.chips == 950);

    setupDeal(&game, 5, 10, 6, 7, 10);
    dealRound(&game);
    doubleDown(&game);
    assert(game.phase == DEALER_TURN && game.chips == 900 && game.bet == 100 && handValue(&game.player) == 21);
    dealerStep(&game, 1.0f);
    assert(game.phase == ROUND_OVER && game.result == PLAYER_WINS && game.chips == 1100);
    nextRound(&game);
    assert(game.phase == BETTING && game.bet == 50);

    setupDeal(&game, 10, 1, 8, 6, 5);
    dealRound(&game);
    assert(game.phase == INSURANCE);
    insuranceChoice(&game, false);
    stand(&game);
    dealerStep(&game, 1.0f);
    assert(game.phase == ROUND_OVER && game.result == PLAYER_WINS && game.dealer.count == 2 && game.chips == 1050);

    setupDeal(&game, 10, 9, 7, 6, 10);
    dealRound(&game);
    stand(&game);
    dealerStep(&game, 1.0f);
    assert(game.phase == DEALER_TURN && game.dealer.count == 3);
    dealerStep(&game, 1.0f);
    assert(game.result == PLAYER_WINS && game.chips == 1050);

    setupDeal(&game, 10, 10, 7, 7, 2);
    dealRound(&game);
    stand(&game);
    dealerStep(&game, 1.0f);
    assert(game.result == PUSH && game.chips == 1000);

    setupDeal(&game, 10, 9, 5, 8, 2);
    game.chips = 50;
    dealRound(&game);
    assert(game.chips == 0 && game.phase == PLAYER_TURN);
    doubleDown(&game);
    assert(game.phase == PLAYER_TURN && game.player.count == 2 && game.bet == 50);
    hit(&game);
    doubleDown(&game);
    assert(game.player.count == 3 && game.bet == 50);

    setupDeal(&game, 10, 1, 9, 10, 2);
    dealRound(&game);
    insuranceChoice(&game, true);
    assert(game.phase == ROUND_OVER && game.result == DEALER_WINS && game.chips == 1000);
    assert(game.insuranceBet == 25 && game.roundStartChips == 1000);

    setupDeal(&game, 1, 1, 10, 13, 2);
    dealRound(&game);
    insuranceChoice(&game, true);
    assert(game.result == PUSH && game.chips == 1050);

    setupDeal(&game, 10, 1, 8, 6, 5);
    dealRound(&game);
    insuranceChoice(&game, true);
    assert(game.phase == PLAYER_TURN && game.chips == 925);
    stand(&game);
    dealerStep(&game, 1.0f);
    assert(game.result == PLAYER_WINS && game.chips == 1025);

    setupDeal(&game, 10, 9, 6, 7, 4);
    dealRound(&game);
    surrender(&game);
    assert(game.phase == ROUND_OVER && game.result == PLAYER_SURRENDER && game.chips == 975);

    setupDeal(&game, 8, 10, 8, 7, 3);
    game.deck[5] = (Card){10, 3};
    game.deck[6] = (Card){10, 2};
    dealRound(&game);
    splitPair(&game);
    assert(game.split && !game.splitAces && game.chips == 900 && game.bet == 50 && game.splitBet == 50);
    assert(handValue(&game.player) == 11 && handValue(&game.splitHand) == 18);
    doubleDown(&game);
    assert(game.activeHand == 1 && game.bet == 100 && game.chips == 850 && handValue(&game.player) == 21);
    stand(&game);
    assert(game.phase == DEALER_TURN);
    dealerStep(&game, 1.0f);
    assert(game.result == PLAYER_WINS && game.splitResult == PLAYER_WINS && game.chips == 1150);

    setupDeal(&game, 1, 10, 1, 7, 13);
    game.deck[5] = (Card){9, 2};
    dealRound(&game);
    splitPair(&game);
    assert(game.splitAces && game.phase == DEALER_TURN);
    dealerStep(&game, 1.0f);
    assert(game.result == PLAYER_WINS && game.splitResult == PLAYER_WINS && game.chips == 1100);

    setupDeal(&game, 8, 10, 8, 7, 13);
    game.deck[5] = (Card){13, 2};
    dealRound(&game);
    splitPair(&game);
    assert(game.activeHand == 0);
    stand(&game);
    assert(game.activeHand == 1);
    surrender(&game);
    assert(game.phase == PLAYER_TURN && !game.surrendered);
    stand(&game);
    dealerStep(&game, 1.0f);
    assert(game.phase == ROUND_OVER && game.result == PLAYER_WINS && game.splitResult == PLAYER_WINS);

    assert(resetGame(&game));
    changeBet(&game, 100000);
    assert(game.bet == 1000);
    game.chips = 50000;
    changeBet(&game, 100000);
    assert(game.bet == MAX_BET);

    unsigned int choices = 1234567u;
    assert(resetGame(&game));
    for (int round = 0; round < 1000; round++) {
        if (game.chips < MIN_BET) { assert(resetGame(&game)); }
        if (game.phase == ROUND_OVER) { nextRound(&game); }
        assert(game.phase == BETTING && game.bet >= MIN_BET && game.bet <= game.chips);
        dealRound(&game);
        if (game.phase == INSURANCE) {
            choices = choices * 1664525u + 1013904223u;
            insuranceChoice(&game, (choices & 1u) != 0);
        }
        for (int step = 0; step < 100 && game.phase != ROUND_OVER; step++) {
            if (game.phase == DEALER_TURN) { dealerStep(&game, 1.0f); }
            else if (game.phase == PLAYER_TURN) {
                choices = choices * 1664525u + 1013904223u;
                Hand *hand = activePlayerHand(&game);
                if (!game.split && hand->count == 2 && hand->cards[0].rank == hand->cards[1].rank && game.chips >= game.bet) { splitPair(&game); }
                else if (handValue(hand) < 15) { hit(&game); }
                else if ((choices & 7u) == 0 && hand->count == 2 && !game.split) { surrender(&game); }
                else { stand(&game); }
            }
            assert(game.nextCard >= 0 && game.nextCard <= DECK_SIZE);
            assert(game.player.count <= HAND_SIZE && game.splitHand.count <= HAND_SIZE && game.dealer.count <= HAND_SIZE);
            assert(game.chips >= 0);
        }
        assert(game.phase == ROUND_OVER && game.result != NO_RESULT);
        assert(!game.split || game.splitResult != NO_RESULT);
        bool inPlay[4][14] = {{false}};
        const Hand *hands[] = {&game.player, &game.splitHand, &game.dealer};
        for (int h = 0; h < 3; h++) {
            for (int i = 0; i < hands[h]->count; i++) {
                Card card = hands[h]->cards[i];
                assert(card.suit >= 0 && card.suit < 4 && card.rank >= 1 && card.rank <= 13);
                assert(!inPlay[card.suit][card.rank]);
                inPlay[card.suit][card.rank] = true;
            }
        }
    }
    return(0);
}
